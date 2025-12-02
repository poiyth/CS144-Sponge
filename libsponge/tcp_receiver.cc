#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {

    //如果排序器结束了就直接返回。坑点！！！！
    if(_reassembler.end()) return;

    const TCPHeader &header = seg.header();
    //如果设置了syn,就在第一次见到syn设置包时设置他，如果不是第一次syn，检查是否相同，不同则返回。
    if(header.syn) {
        if(!_isn.has_value()) {
            _isn = header.seqno;
        }
        else {
            if(header.seqno != _isn.value())
                return;
        }
    } //如果不是syn并且isn没设置，说明是非法数据报，直接丢弃。
    else {
        if(!_isn.has_value())
            return;
    }

    //能来这里的只有设置了syn：1.第一次来设置。2.第二次来设置且isn和上次相同。或者没设置syn但isn早已有值。
    //这里设置是否收到fin了
    bool end = header.fin;
    //这里向字符串重组器推送数据
    const std::string payload = seg.payload().copy();
    //这里为了防止syn数据包携带数据
    WrappingInt32 seqno(header.seqno.raw_value());
    if(header.syn) seqno = seqno + static_cast<uint32_t>(1);
    //推送数据
    _reassembler.push_substring(payload, unwrap(seqno, _isn.value(), _reassembler.get_index()) - 1, end);

}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    optional<WrappingInt32> ackno{std::nullopt};

    //这里的坑点，如果已经接收到FIN报文并且排序结束了，则我们的绝对序列号和实际序列号seqno实际上错开了一位这里补上，
    //并且这里FIN表示连接结束，所以不会再收到数据包，后续没有序列号之间的转换了，这里单纯加一不影响
    uint64_t ab_seqno = _reassembler.get_index() + 1;
    if(_reassembler.end()) ab_seqno ++;

    if(_isn.has_value()) //这里注意get_index返回的是字节流号，先+1转换成绝对字节流号，再wrap转换成seqno
        ackno = wrap(ab_seqno, _isn.value());
    return ackno;
}

//这里返回排序器的窗口大小即允许容纳未排序的字节个数。
size_t TCPReceiver::window_size() const { 
    return _reassembler.get_window();
}
