#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : cap(capacity), readed(0), written(0), write_index(0), input_end(false), buf({}) {}

size_t ByteStream::write(const string &data) {
    size_t write_num = min(data.length(), remaining_capacity());
    for (size_t i = 0; i < write_num; ++i) {
        buf.push_back(data[i]);
    }
    written += write_num;
    write_index += write_num;
    return write_num;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t to_read = len;
    if (len > buffer_size()) {
        to_read = buffer_size();
    }
    string res;
    for (size_t i = 0; i < to_read; ++i) {
        res += buf[i];
    }
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (buffer_size() <= 0) {
        return;
    }
    size_t to_pop = len;
    if (len > buffer_size()) {
        to_pop = buffer_size();
    }
    write_index -= to_pop;
    buf.erase(buf.begin(),buf.begin()+static_cast<int>(to_pop));
    //    buf.erase(buf.begin(),buf.begin()+to_pop);
    readed += to_pop;

}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    if (buffer_size() <= 0) {
        return "";
    }
    string res;
    res = peek_output(len);
    pop_output(len);
    return res;
}

void ByteStream::end_input() { input_end = true; }

bool ByteStream::input_ended() const { return input_end; }

size_t ByteStream::buffer_size() const { return write_index; }

bool ByteStream::buffer_empty() const { return write_index == 0; }

bool ByteStream::eof() const { return write_index == 0 && input_end; }

size_t ByteStream::bytes_written() const { return written; }

size_t ByteStream::bytes_read() const { return readed; }

size_t ByteStream::remaining_capacity() const { return cap - write_index; }