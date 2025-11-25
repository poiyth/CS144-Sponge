#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) 
    , _rto{_initial_retransmission_timeout}{}

//放回未确认队列中的总字节数
uint64_t TCPSender::bytes_in_flight() const { 
    //暴力便利队列中的每个字节段
    uint64_t tot_bytes = 0;
    for(const auto &seg : _outstanding_segments) 
        tot_bytes += seg.length_in_sequence_space();
    return tot_bytes;
}

//填充窗口操作
void TCPSender::fill_window() {
    //这里确保window_size至少为1，可以发送syn探测报文
    uint16_t window_size = _window_size;
    if(!window_size) window_size = 1;

    //如果未确认队列总字节数 < 收到的窗口大小，且还有要发送的字节，就发送tcp段
    uint64_t tot_bytes;
    while((tot_bytes = bytes_in_flight()) < window_size) {
        //创建tcp段，并写入序列号
        TCPSegment seg;
        seg.header().seqno = next_seqno();

        //先写入syn字段
        if(_next_seqno == 0) {
            seg.header().syn = true;
            tot_bytes += 1;
        }        
        //接下来构造数据部分,这里bytes计算需要从stream中拿出多少，需要满足
        size_t bytes = min(window_size - tot_bytes, _stream.buffer_size());
        bytes = min(bytes, TCPConfig().MAX_PAYLOAD_SIZE);
        seg.payload() = Buffer(_stream.read(bytes));
        tot_bytes += bytes;

        //最后尝试写入fin字段?需要字节流结束并且当前字节还能容纳
        //坑点，这里需要注意之前没有发送过fin才行
        if(!_fin_sent && _stream.eof() && tot_bytes < window_size) {
            seg.header().fin = true;
            tot_bytes += bytes;
            _fin_sent = true;
        } 

        //如果是在没事干了，这就直接退出。
        if(seg.length_in_sequence_space() == 0) break;

        //放入输出队列和未确认队列中
        _segments_out.push(seg);
        _outstanding_segments.push_back(seg);
        //设定计时器
        if(!_timer_running) {
            _timer_running = true;
            _time_elapsed = 0;
        }
        _next_seqno += seg.length_in_sequence_space();
    }
}

//接收到的ackno 和 窗口大小
//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    //首先检查ackno是否合规,
    if(next_seqno().raw_value() < ackno.raw_value()) return;

    //第一步，清理未确认队列中确认的tcp段
    while (!_outstanding_segments.empty()) {
        const TCPSegment tcp = _outstanding_segments.front();
        //这里用seqno+数据负载长度确认是否该tcp段全部数据都被ack了，注意syn,fin数据报比较特殊，要+1
        if(tcp.header().seqno.raw_value() + tcp.length_in_sequence_space() > ackno.raw_value()) 
            break;
        //将确认过的tcp段出队
        _outstanding_segments.pop_front();
        //更新时钟，累计传输次数和rto
        _time_elapsed = 0;
        _retransmission_count = 0;
        _rto = _initial_retransmission_timeout;
    }
    //如果未确认队列为空，则将计时器关闭。
    if(_outstanding_segments.empty()) _timer_running = false;

    //更新窗口信息并填充窗口,窗口大小至少为1
    _window_size = window_size;
    fill_window();
    
}

//计时功能，所有者告知TCP sender过去了多少时间，TCP sender应该自主负责所有超时事务
//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    //如果计时器没打开，则直接返回
    if(!_timer_running) return;

    //累计计时器时间
    _time_elapsed += ms_since_last_tick;
    //超时，启动重传服务
    if(_time_elapsed >= _rto) {
        //先重设状态，之后拿出队头，重新传输
        _time_elapsed = 0;
        //这里只有窗口大小不为0时才可翻倍rto和增加连续重传次数。
        if(_window_size != 0) {
            _rto *= 2;
            _retransmission_count ++;
        }
        _segments_out.push(_outstanding_segments.front());
        return;
    }
}

//返回连续重传次数
unsigned int TCPSender::consecutive_retransmissions() const { 
    return _retransmission_count; 
}

//发送一个空的tcp段，仅将seqno设置正确，不消耗seqno
void TCPSender::send_empty_segment() {
    //创建seg，且正确设置seqno
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    //将其输出，且不需要跟踪。
    _segments_out.push(seg);
    return;
}
