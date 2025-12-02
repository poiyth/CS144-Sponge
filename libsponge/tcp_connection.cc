#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.
//入站流和出战流都是TCPConnection作为TCPSocket整体，对于应用程序而言。
//即入站流是TCPReceiver中的stream_out，即外部网络给当前应用程序的字节流。
//出站流是TCPSender中的stream_in，即应用程序给外部网络发送的字节流。

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//返回出站流的容量，直接返回出站流容量即可。
size_t TCPConnection::remaining_outbound_capacity() const { 
     return _sender.stream_in().remaining_capacity();
}

//已经发送但尚未确认的字节数，即TCPSender中未确认队列中的字节数
size_t TCPConnection::bytes_in_flight() const { 
    return _sender.bytes_in_flight(); 
}

//已经收到，但未排序的字节数，即TCPReceiver中未排序字节数
size_t TCPConnection::unassembled_bytes() const { 
    return _receiver.unassembled_bytes();
}

//自从上次收到tcp段，已经过去的时间,未实现
size_t TCPConnection::time_since_last_segment_received() const { 
    return _time_since_last_received_counter;
}

//接收一个TCP段
void TCPConnection::segment_received(const TCPSegment &seg) { 
    //如果设置了重置标记，则将入站和出战设置为错误状态，并终止连接。
    if(seg.header().rst) {
        //入站流关闭
        _receiver.stream_out().set_error();
        //出战流关闭
        _sender.stream_in().set_error();
        return;
    }

    //否则
    //这里先更新一下时间
    _time_since_last_received_counter = 0;
    //将tcp段给TCPReceiver
    _receiver.segment_received(seg);
    //这里判断如果是非法数据，应该直接丢弃
    if(!_receiver.active()) return;
    //如果设置了ACK，同时更新信息到TCPSender
    if(seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }
    //如果对方发来的tcp段，占用了序列号，就需要显性回应。
    //采取的措施首先是尝试fill window.如果sender没有要发送的数据捎带ackno和当前窗口信息，则发送空段回复
    if(seg.length_in_sequence_space() != 0) {
        _sender.fill_window();
        //判断sender有无数据可以捎带ackno和窗口信息，如果没有发送空段
        if(_sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
        push_sender(); //显性的推一下
    }

    //收到“保持活动”的tcp段，即发送方不占用任何字节，且当前已经设置了ackno，且发送方序列号=ackno-1
    //对方只是想知道当前的seqno和window_size
    if(_receiver.ackno().has_value() && seg.length_in_sequence_space() == 0  
       && seg.header().seqno == (_receiver.ackno().value() - static_cast<uint32_t>(1))) {
        _sender.send_empty_segment();  //先发送一个空的段
        push_sender();                 //显性推送一下
    }

    //如果入站流在出战流到达eof前收到结束信号，说明当前是被动关闭放，需要将_linger_after_streams_finish变量置为false
    if(_receiver.stream_out().input_ended() && !_sender.stream_in().input_ended()) {
        _linger_after_streams_finish = false;
    }
}

//TCP连接是否应该存活
//1.如果一旦收到RST或者自己主动发送RST
//2.所有正常流程结束，正常结束。
//否则返回true。
bool TCPConnection::active() const { 
    //1.首先判断是否接受到RST，不正常断开连接
    if(_sender.stream_in().error() || _receiver.stream_out().error()) {
        return false;
    }

    //2.干净的结束
    bool prerequisites = _receiver.stream_out().eof() && _receiver.unassembled_bytes() == 0 &&   //条件1，入站流已经结束且排序器也为空
                         _sender.stream_in().eof() && _sender.fin_sent() &&                      //条件2 出战流结束且fin报文发送过了
                         _sender.bytes_in_flight() == 0;                                         //条件3，出战流全部被确认
    if(prerequisites) {
        //如果自己不是主动关闭的一方,直接关闭即可
        if(!_linger_after_streams_finish) 
            return false;
        else { //否则若是主动关闭的一方，需要等待时间。
            TCPConfig cfg;
            if(time_since_last_segment_received() >= 10 * cfg.rt_timeout)
                return false;
            else 
                return true;
        }
    }
    return true;
}

//应用层的数据发送
size_t TCPConnection::write(const string &data) {
    //向发送器的可靠字节流中写入
    size_t tot_writen=_sender.stream_in().write(data);
    _sender.fill_window();
    push_sender();            //向前推。
    return tot_writen;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
//tick函数实现
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    //累计上次收到received的时间
    _time_since_last_received_counter += ms_since_last_tick;
    //更新发送起的时间
    _sender.tick(ms_since_last_tick);
    //如果重传次数过多则终止连接
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        //首先将入站流和出站流关闭
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        //这里直接将sender的输出队列清空，因为不需要了
        _sender.clear_sender();
        //其次向对方发送带有rst标志的空段
        _sender.send_empty_segment();
        //这里发送的空段在队尾,设置rst=1
        _sender.segments_out().back().header().rst = true;
    }
    //这里可能导致sender超时重传，所以需要显性的推一下，这里统一推一下
    push_sender();
}

//关闭入站字节流
void TCPConnection::end_input_stream() {
    //首先关闭入站字节流
    _sender.stream_in().end_input();
    _sender.fill_window();           //填充窗口，将FIN字段送入写方输出字节流
    push_sender();                   //显性推送
}

//发送一个SYN数据段
//由于TCPSender的初始化时窗口大小只有1，所以直接调用fill window窗口节课。
void TCPConnection::connect() {
    //第一步使其填充窗口，由于初始化窗口大小只有1，所以就是发送SYN报文
    _sender.fill_window();

    //第二步，将TCPSender发送的TCP段转至TCPConnection的发送队列,调用函数即可
    push_sender();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            //这里直接将sender的输出队列清空，因为不需要了
            _sender.clear_sender();
            //其次向对方发送带有rst标志的空段
            _sender.send_empty_segment();
            //这里发送的空段在队尾,设置rst=1
            _sender.segments_out().back().header().rst = true;
            push_sender();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

//将TCPSender发送的TCP段转至TCPConnection的发送队列,并加上必要标识
void TCPConnection::push_sender() {
    //sender的发送端不空
    while(!_sender.segments_out().empty()) {
        //先从TCPSender中取出，再将其从TCPSender中剔除
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        //在交给OS前，TCPConnection需要填充TCPSender填充不了的字段，比如ackno和window
        //ackno需要有才能赋值
        if(_receiver.ackno().has_value()) {
            seg.header().ackno = _receiver.ackno().value();
            seg.header().ack = true;
        }
        //window_size是TCPReceiver自身的属性，直接复制即可。
        seg.header().win = _receiver.window_size();
        _segments_out.push(seg);
    }
}