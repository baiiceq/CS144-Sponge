#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (stream_out().input_ended()) { 
        return;
    }

    if (!_syn_received) {  // 如果还未收到第一个SYN
        if (seg.header().syn) {
            _isn = seg.header().seqno;  // 保存isn
            _syn_received = true;
        } else { 
            return;
        }
    } else {
        if (seg.header().syn) {   // 如果报文段带SYN，则出现错误
            stream_out().set_error();
            return;
        }
    }

    // 获取数据长度
    size_t data_length = seg.length_in_sequence_space();
    if (seg.header().fin) {
        data_length--;
    }

    if (seg.header().syn) {
        data_length--;
    }

    // 获取第一个字节的字节流序号
    uint32_t n = seg.header().seqno.raw_value() + static_cast<uint32_t>(seg.header().syn);
    size_t abs_seqno = unwrap(WrappingInt32(n), _isn, stream_out().bytes_written() + 1);

    // 组装数据
    _reassembler.push_substring(seg.payload().copy(), abs_seqno - 1, seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const { 
    if (!_syn_received) {
        return std::nullopt;
    }

    // SYN 占用了第一个序列号
    size_t abs_seqno = stream_out().bytes_written() + 1;

    if (stream_out().input_ended()) {
        // FIN 占用了最后一个序列号
        abs_seqno++;
    }

    return wrap(abs_seqno, _isn); 
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
