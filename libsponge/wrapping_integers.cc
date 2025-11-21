#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
//绝对序列号转换成序列号seqno
//这里对绝对序号进行强制转换成32位相当于%2^32,符合seqno从0到2^32-1的循环，最后加上isn即可
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return isn + static_cast<uint32_t>(n);
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
//给定序列号seqno、isn和绝对序列号的checkpoint，转换成绝对序列号
//坑点绝对序列号0-最大值之间，不能上溢或下溢
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    //先取出绝对序号的后32位
    uint64_t offset =static_cast<uint64_t>(static_cast<uint32_t>(n - isn));

    //构造粗略的候选者
    uint64_t candidate = (checkpoint & 0xFFFFFFFF00000000) | offset;
    uint64_t wrap_const = 1ul << 32;
    //还有一个候选者是candidate + 0xFFFFFFFF
    if(candidate < checkpoint) {
        //这里考虑candidate到checkpoint距离超过一半，则一定是另一个
        if(checkpoint - candidate > wrap_const / 2 && candidate + wrap_const > candidate) 
            return candidate + wrap_const;
    }
    //还有一个候选者是candidate-wrap_const
    else {
        //同理
        if(candidate - checkpoint > wrap_const / 2 && candidate > wrap_const)
            return candidate - wrap_const;
    }
    return candidate;
}
