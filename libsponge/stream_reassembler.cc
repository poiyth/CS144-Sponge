#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {

    //已经收到结束的字节信号并且字节序已经写到结束信号了，就不允许继续插入了。
    if(_eof && _index == _last_index) return;
    if(eof)
    {
        _eof = eof;
        _last_index = index + data.length(); //这里记录最后的字节
    }

    //首先尽可能的将字节存入map中，首先计算能接受的窗口区间
    size_t window_l = _index;
    size_t window_r = _index + _capacity - _output.buffer_size() - 1;

    for(size_t i = 0; i < data.length(); i++) {
        if(index + i < window_l) continue;
        if(index + i > window_r) break;
        _buffer[index + i] = data[i];
    }

    //由于有新插入的，所以可以更新_buffer到有序的字节流中
    while(_buffer.find(_index) != _buffer.end()) {
        std::string st(1, _buffer[_index]);
        _buffer.erase(_index);
        _index += _output.write(st);
    }
    
    //如果收到结束信号且已经写完了，关闭ByteStream
    if(_eof && _index == _last_index) {
        _output.end_input();
    }
    
}

//返回目前还没有排序的字节数
size_t StreamReassembler::unassembled_bytes() const { 
    return _buffer.size();
}

bool StreamReassembler::empty() const { 
    return _buffer.empty(); 
}
