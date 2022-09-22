#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (seg.header().rst) {
        stream_out().set_error();
        return;
    }
    if (stream_out().error()) {
        return;
    }
    size_t stream_offset = 1;
    if (!_receive_syn) {
        if (!seg.header().syn) {
            return;
        }
        _receive_syn = true;
        _isn = seg.header().seqno;
        if (seg.payload().size() > 0 || seg.header().fin) {
            stream_offset = 0;
        }
    }

    auto seq = seg.header().seqno;
    auto absolute_seq = unwrap(seq, _isn, stream_out().bytes_written());

    _reassembler.push_substring(string(seg.payload().str()), absolute_seq - stream_offset, seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_receive_syn) {
        return nullopt;
    }
//    size_t syn = 1;
    size_t fin = stream_out().input_ended() ? 1 : 0;
    return wrap(stream_out().bytes_written() + 1 + fin, _isn);
}

size_t TCPReceiver::window_size() const { return _reassembler.window_size(); }
