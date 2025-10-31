#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {  return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time = 0;
    if (seg.header().rst) {
        unclean_shutdown();
        return;
    }
    _receiver.segment_received(seg);

    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    if (seg.length_in_sequence_space() > 0) {
        _sender.fill_window();
        if (_sender.segments_out().empty()){
            _sender.send_empty_segment();
        }
    }

    flush_send();
    clean_shutdown();
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t write_size = _sender.stream_in().write(data);
    _sender.fill_window();
    flush_send();
    return write_size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) { 
    if (!_active) {
        return;
    }

    _time += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);

    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        unclean_shutdown();
        _sender.fill_window();
        _send_rst_segment();
    }

    flush_send();
    // 可能会结束
    clean_shutdown();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    flush_send();
    clean_shutdown();
}

void TCPConnection::connect() {
    _sender.fill_window();
    flush_send();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _send_rst_segment();
            unclean_shutdown();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::unclean_shutdown() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = false;
}

void TCPConnection::clean_shutdown() {
    if (_receiver.unassembled_bytes() == 0 && _receiver.stream_out().input_ended()) {
        if (!_sender.stream_in().eof()) {
            _linger_after_streams_finish = false;
        }
        if (_sender.stream_in().eof() && _sender.bytes_in_flight() == 0) {
            if (_time >= 10 * _cfg.rt_timeout || !_linger_after_streams_finish) {
                _active = false;
            }
        }
    }
}

void TCPConnection::flush_send() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _setup_segment(seg);
        _segments_out.push(seg);
        _sender.segments_out().pop();
    }
}

void TCPConnection::_send_rst_segment() {
    if (_sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }

    TCPSegment seg = _sender.segments_out().front();
    _setup_segment(seg);
    seg.header().rst = true;
    _segments_out.push(seg);
    _sender.segments_out().pop();
}

void TCPConnection::_setup_segment(TCPSegment& seg) {
    if (_receiver.ackno().has_value()) {
        seg.header().ack = true;
        seg.header().ackno = _receiver.ackno().value();
    }

    seg.header().win = _receiver.window_size();
}

