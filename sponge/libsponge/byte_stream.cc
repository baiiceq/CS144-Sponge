#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity), 
                                                _size(0), _input_end(false) {
    _buf.clear();
}

size_t ByteStream::write(const string &data) {
    if(_input_end) {
        return 0;
    }

    size_t write_length = data.size();
    if(write_length > _capacity - _size) {
        write_length = _capacity - _size;
    }

    _buf += data.substr(0, write_length);
    _size += write_length;
    _write_bytes += write_length;
    return write_length;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string output = _buf.substr(0, min(_size, len));
    return output;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t pop_len = min(_size, len);
    _buf.erase(0, pop_len);
    _size -= pop_len;
    _read_bytes += pop_len;  
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string result = peek_output(len); 
    pop_output(len);                   
    return result; 
}

void ByteStream::end_input() {
    _input_end = true;
}

bool ByteStream::input_ended() const { return _input_end;}

size_t ByteStream::buffer_size() const { return _size; }

bool ByteStream::buffer_empty() const { return _size == 0; }

bool ByteStream::eof() const { return _size == 0 && _input_end; }

size_t ByteStream::bytes_written() const { return _write_bytes; }

size_t ByteStream::bytes_read() const { return _read_bytes; }

size_t ByteStream::remaining_capacity() const {
    return _capacity - _size;
}