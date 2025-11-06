#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//构造函数用初始化列表进行初始化
ByteStream::ByteStream(const size_t capacity) 
    :_buffer(capacity), _capacity(capacity), _buffer_remain(capacity) {}

//往流中写一个字符串
size_t ByteStream::write(const string &data) {
    size_t count = 0; //统计写了多少个
    size_t lop_ct = min(data.length(), _buffer_remain);
    _tot_write += lop_ct;
    //能写的情况下尽可能的写，写不下的直接抛弃。
    while (count < lop_ct) {
       _buffer[_buffer_write] = data[count];
       _buffer_write = (_buffer_write + 1) % _capacity;
       _buffer_remain--;
       count++;
    }
    
    return count;
}

//读取端拿出len个
//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    std::string output;
    size_t count = 0, lop_ct = min(len, _capacity - _buffer_remain);
    //注意这里的循环条件，应该是读取的长度和剩余个数取min
    while(count < lop_ct) {
        output.push_back(_buffer[(_buffer_read + count)%_capacity]);
        count++;
    }
    return output;
}

//读取段删除len个，读取指针往前移动len个
//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    size_t count = 0;
    size_t lop_ct = min(len, _capacity - _buffer_remain);
    _tot_read +=lop_ct;
    while(count < lop_ct)
    {
        _buffer_read = (_buffer_read + 1)%_capacity;
        count++;
        _buffer_remain++;
    }
}

//读取len个并删除，在读取的同时移动读取指针即可。
//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string output;
    size_t count = 0, lop_ct = min(len, _capacity - _buffer_remain);
    _tot_read += lop_ct;
    //注意这里的循环条件，应该是读取的长度和剩余个数取min
    while(count++ < lop_ct) {
        output.push_back(_buffer[_buffer_read]);
        _buffer_read = (_buffer_read + 1)%_capacity;
        _buffer_remain++;
    }
    return output;
}

//写入方是否结束，用bool标记即可。
void ByteStream::end_input() {
    _end = true;    
}

//询问写入放是否结束。
bool ByteStream::input_ended() const { 
    return  _end;
}

//当前能够读取的最大数量，用总容量-剩余容量=当前容量中有的个数
size_t ByteStream::buffer_size() const {
     return _capacity - _buffer_remain; 
}

//当前容器是否为空,用剩余容量是否为容器容量判断即可
bool ByteStream::buffer_empty() const { 
    return _buffer_remain == _capacity ? true : false; 
}

//是否到文件末尾，需要写方结束并且剩余为0
bool ByteStream::eof() const { 
    if(_end && _buffer_remain == _capacity) return true;
    return false; 
}

//一共写的个数
size_t ByteStream::bytes_written() const { 
    return _tot_write; 
}

//一共读的个数
size_t ByteStream::bytes_read() const { 
    return _tot_read; 
}

//剩余容量直接返回
size_t ByteStream::remaining_capacity() const {
     return _buffer_remain; 
}
