#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _retransmission_timeout(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t res = 0;
    for (auto item : _outstanding_seg) {
        res += item.GetSeg().length_in_sequence_space();
    }
    return res;
}

void TCPSender::fill_window() {
    if (_fin || send_size() <= 0) {
        return;
    }
    if (_syn) {
        _syn = false;
        push_data(std::string{}, true, false);
        start_timer();
        return;
    } else if (_stream.eof()) {
        _fin = true;
        push_data(std::string{}, false, true);
        start_timer();
        return;
    }
    auto remain = send_size();
    while (remain > 0 && !_fin && !_stream.buffer_empty()) {
        auto len = min(remain, min(TCPConfig::MAX_PAYLOAD_SIZE, _stream.buffer_size()));
        auto data = _stream.read(len);
        int fin = 0;
        if (len < remain && _stream.eof()) {
            _fin = true;
            fin = 1;
        }
        push_data(data, false, fin);
        remain -= (len + fin);
    }

    start_timer();
}
void TCPSender::start_timer() {
    if (!_retrans_timer._running) {
        _retrans_timer._running = true;
        _retrans_timer._expire_time = _logical_time + _retransmission_timeout;
    }
}
void TCPSender::push_data(const string &read_str, bool syn, bool fin) {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    segment.header().syn = syn;
    segment.header().fin = fin;
    segment.payload() = string(read_str);
    _segments_out.push(segment);
    OutStandingSegment outStandingSegment{segment, _push_index};
    ++_push_index;
    _outstanding_seg.insert(outStandingSegment);
    _next_seqno += segment.length_in_sequence_space();
    //    _window_size -= read_str.length() + fin;
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = window_size;
    auto abs_ack = unwrap(ackno, _isn, _next_seqno);
    if (abs_ack > next_seqno_absolute()) {
        return ;
    }
    if (abs_ack > _largest_ackno) {
        //! 1.set rto to initial rto
        _retransmission_timeout = _initial_retransmission_timeout;
        _largest_ackno = abs_ack;
        _consecutive_retransmissions = 0;

        vector<OutStandingSegment> remove_vec;
        for (auto out_seg : _outstanding_seg) {
            auto abs_seq = unwrap(out_seg.GetSeg().header().seqno, _isn, _next_seqno);
            if (abs_seq + out_seg.GetSeg().length_in_sequence_space() <= abs_ack) {
                remove_vec.push_back(out_seg);
            }
        }
        for (const auto &item : remove_vec) {
            _outstanding_seg.erase(item);
        }
        if (_outstanding_seg.empty()) {
            _retrans_timer._running = false;
        } else {
            _retrans_timer._running = true;
            _retrans_timer._expire_time = _logical_time + _retransmission_timeout;
        }
    }
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _logical_time += ms_since_last_tick;
    if (_retrans_timer._running and _logical_time >= _retrans_timer._expire_time) {
        //! resend earliest seg
        auto seg = *_outstanding_seg.begin();
        //        _outstanding_seg.erase(_outstanding_seg.begin());
        _segments_out.push(seg.GetSeg());

        //! 拥塞控制
        if (_window_size > 0) {
            _consecutive_retransmissions++;
            _retransmission_timeout *= 2;
        }

        //! reset expire timer
        _retrans_timer._expire_time = _logical_time + _retransmission_timeout;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
}

uint64_t TCPSender::send_size() {
    uint64_t window = _window_size > 0 ? _window_size : 1;
    return window - (_next_seqno - _largest_ackno);
}
