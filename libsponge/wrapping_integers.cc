#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return WrappingInt32{static_cast<uint32_t>(n) + isn.raw_value()}; }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own _ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one _ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different _ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t offset = static_cast<uint64_t>(n - isn);
    uint64_t round = checkpoint >> 32;
    uint64_t range = 1ul << 32;

    uint64_t offset_1 = offset + round * range;
    uint64_t offset_2 = round == 0 ? offset_1 : (round - 1) * range + offset;
    uint64_t offset_3 = offset + (round + 1) * range;

    uint64_t gap_1 = offset_1 > checkpoint ? offset_1 - checkpoint : checkpoint - offset_1;
    uint64_t gap_2 = offset_2 > checkpoint ? offset_2 - checkpoint : checkpoint - offset_2;
    uint64_t gap_3 = offset_3 > checkpoint ? offset_3 - checkpoint : checkpoint - offset_3;

    uint64_t res = 0, min_gap = 0;
    min_gap = min(min(gap_1, gap_2), gap_3);
    if (min_gap == gap_1)
        res = offset_1;
    if (min_gap == gap_2)
        res = offset_2;
    if (min_gap == gap_3)
        res = offset_3;
    return res;
}
