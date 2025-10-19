#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {_retransmission_timeout = _initial_retransmission_timeout; }

// 所有报文段的和，包括SYN和FIN`
uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    // 还没有发送 SYN
    if (_next_seqno == 0) {
        TCPSegment syn_seg;
        syn_seg.header().syn = true;
        syn_seg.header().seqno = _isn;

        _segments_out.push(syn_seg);
        _cache_segment(syn_seg);

        _next_seqno += syn_seg.length_in_sequence_space();
        _bytes_in_flight += syn_seg.length_in_sequence_space();
        return;  // SYN 发送后，本次填充先停
    }

    // 计算当前窗口大小
    size_t win = _windows_size == 0 ? 1 : _windows_size;  // 窗口为0时允许发送1字节
    // 减去已经发生但未确认的长度
    size_t window_left = win > _bytes_in_flight ? win - _bytes_in_flight : 0;

    // 循环填充窗口
    while (window_left > 0) {
        // 流数据还有
        if (!stream_in().eof()) {
            size_t to_read = std::min({window_left, stream_in().buffer_size(), TCPConfig::MAX_PAYLOAD_SIZE});
            if (to_read == 0) break;

            std::string data = _stream.read(to_read);
            if (data.empty()) break;

            TCPSegment seg;
            seg.header().seqno = next_seqno();
            seg.payload() = Buffer(std::move(data));

            if (stream_in().eof() && !_fin_sent && window_left >= seg.length_in_sequence_space() + 1) {
                seg.header().fin = true;
                _fin_sent = true;
            }

            _segments_out.push(seg);
            _cache_segment(seg);

            size_t seg_len = seg.length_in_sequence_space();
            _next_seqno += seg_len;
            _bytes_in_flight += seg_len;
            window_left -= seg_len;
        }
        // 流结束且 FIN 尚未发送
        else if (!_fin_sent) {
            TCPSegment fin_seg;
            fin_seg.header().seqno = next_seqno();
            fin_seg.header().fin = true;

            _segments_out.push(fin_seg);
            _cache_segment(fin_seg);

            size_t seg_len = fin_seg.length_in_sequence_space();
            _next_seqno += seg_len;
            _bytes_in_flight += seg_len;

            _fin_sent = true;
            window_left -= seg_len;
        }
        else {
            // 窗口填满或没有数据可以发送
            break;
        }
    }
}

// 收到确认
//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t abs_ackno = unwrap(ackno, _isn, _next_seqno);
    // 如果 ACK 小于已经发送的最老序号，则忽略
    if (abs_ackno > _next_seqno) {
        return;
    }

    _windows_size = window_size; 
    bool recevie_valid = false;

    while (!_segments_cache.empty()) {
        auto &seg = _segments_cache.front();
        uint64_t seg_start = unwrap(seg.header().seqno, _isn, _next_seqno);
        uint64_t seg_end = seg_start + seg.length_in_sequence_space();

        // 如果该段的末尾序号 <= abs_ackno，则已被确认
        if (seg_end <= abs_ackno) {
            recevie_valid = true;
            _bytes_in_flight -= seg.length_in_sequence_space();
            _segments_cache.pop();
        } else {
            break; 
        }
    }

    fill_window();
    if(recevie_valid) {
        _time = 0;               
        _retx_times = 0;   
        _retransmission_timeout = _initial_retransmission_timeout; 
    }
    
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // 所有段均已确认
    if (_segments_cache.empty()) {
        return;
    }

    _time += ms_since_last_tick;
    if (_time >= _retransmission_timeout && _retx_times <= TCPConfig::MAX_RETX_ATTEMPTS) {
        _segments_out.push(_segments_cache.front());
        // TCP 的指数退避算法
        if (_windows_size > 0) {  // window 为0时不触发
            _retransmission_timeout *= 2;
            _retx_times += 1;
        }
        _time = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retx_times; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno=wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}

void TCPSender::_cache_segment(TCPSegment& seg) {
    _segments_cache.push(seg);
}