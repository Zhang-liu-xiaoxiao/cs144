#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (seg.header().rst) {
        unclean_close(false);
        return;
    }
    //! should send empty ack to data;
    bool need_ack = seg.length_in_sequence_space();
    _time_since_last_segment_received = 0;
    _receiver.segment_received(seg);
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        if (need_ack && !_sender.segments_out().empty()) {
            need_ack = false;
        }
    }

    //! if the segement first handshake
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV and
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        connect();
        return;
    }

    //! four wavetimes passive peer receive fin
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV and
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED) {
        _linger_after_streams_finish = false;
    }

    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV and
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED) {
        if (!_linger_after_streams_finish) {
            //! passive close peer don't need time-wait!
            _active = false;
            return;
        }
    }

    //! empty ack;
    if (need_ack) {
        _sender.send_empty_segment();
    }
    push_out_all_segments();
}
void TCPConnection::unclean_close(bool send_seg) {
    if (send_seg) {
        TCPSegment seg;
        seg.header().rst = true;
        _segments_out.push(seg);
    }
    //    push_out_all_segments();
    _active = false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _linger_after_streams_finish = false;
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t len = _sender.stream_in().write(data);
    _sender.fill_window();
    push_out_all_segments();
    return len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!active()) {
        return;
    }
    _time_since_last_segment_received += ms_since_last_tick;
    _logical_time += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        unclean_close(true);
        return;
    }
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV and
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED and _linger_after_streams_finish and
        _time_since_last_segment_received >= _cfg.rt_timeout * 10) {
        _active = false;
        _linger_after_streams_finish = false;
    }
    push_out_all_segments();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    push_out_all_segments();
}

void TCPConnection::connect() {
    _active = true;
    _sender.fill_window();
    push_out_all_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            unclean_close(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
void TCPConnection::push_out_all_segments() {
    while (!_sender.segments_out().empty()) {
        auto i = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            i.header().ackno = _receiver.ackno().value();
            i.header().ack = true;
            i.header().win = _receiver.window_size();
        }
        _segments_out.push(i);
    }
}
