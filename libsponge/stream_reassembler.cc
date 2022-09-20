#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

[[maybe_unused]] StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _buffer(capacity, '\0'), _flag(capacity, false) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t last_unassambled = _output.bytes_written();
    size_t unaccepted = last_unassambled + _capacity;

    if (index > unaccepted || (index + data.length()) < last_unassambled) {
        return;
    }
    size_t start_index = index < last_unassambled ? last_unassambled : index;
    size_t end_index = (index + data.length()) > unaccepted ? unaccepted : (index + data.length());

    for (size_t i = start_index; i < end_index; ++i) {
        if (!_flag[i - last_unassambled]) {
            _buffer[i - last_unassambled] = data.at(i-index);
            _unassembled_bytes++;
            _flag[i - last_unassambled] = true;
        }
    }
    string write_str ;
    while (_flag.front()) {
        write_str += _buffer.front();
        _buffer.pop_front();
        _flag.pop_front();
        _buffer.push_back('\0');
        _flag.push_back(false);
    }
    if (write_str.length()) {
        _unassembled_bytes -= write_str.length();
        _output.write(write_str);
    }
    if (eof) {
        _is_eof = true;
        _eof_index = end_index;
    }
    if (_is_eof && _eof_index == _output.bytes_written()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }