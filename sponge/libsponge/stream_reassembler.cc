#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : 
    _output(capacity), 
    _capacity(capacity),
    _data_map()
{
    ;
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t first_readable = _next_assembled_index - _output.buffer_size();
    size_t first_unacceptable = first_readable + _capacity;

    bool can_accept_all = index + data.size() <= first_unacceptable; 

    if(!_eof && can_accept_all && eof) {
        _eof = true; 
        _eof_index = index + data.size();
    }

    size_t accept_end = index + data.size();
    if(!can_accept_all) {
        accept_end = first_unacceptable;
    }

    if (index > accept_end) {
        return;
    }

    size_t next_unassembled = first_readable + _output.buffer_size();

    // 可直接放入_output
    if(index <= next_unassembled) {
        if(accept_end > next_unassembled) {
            _output.write(data.substr(next_unassembled - index, accept_end - index));
            if(eof && can_accept_all) {
                _output.end_input();
            }
            else {
                auto it = _data_map.begin();
                while(it != _data_map.end() && it->first <= accept_end) {
                    if(accept_end - it->first < it->second.size() && it->first + it->second.size() > accept_end) {
                        _output.write(it->second.substr(accept_end - it->first, it->second.size() - (accept_end - it->first)));
                        accept_end = it->first + it->second.size();
                    }
                    it = _data_map.erase(it);
                }
            }

            _next_assembled_index = accept_end;
            if(_eof && _next_assembled_index >= _eof_index) {
                _output.end_input();
            }
        }
        else if(accept_end == next_unassembled) {
            if(eof && can_accept_all) {
                _output.end_input();
            }
        }
    }
    else if(index > next_unassembled) { // 无法直接放入，判断是否可以和已有项合并
        size_t start = index;
        auto it = _data_map.lower_bound(index);
        bool can_merge_with_prev = false;
        auto it_prev = it;
        if(it != _data_map.begin())
        {
            it = prev(it);
            it_prev = it;
            if(index <= it->first + it->second.size() && accept_end > it->first + it->second.size()) { // 可与前一项合并
                can_merge_with_prev = true;
                it->second += data.substr(it->first + it->second.size() - index, accept_end - (it->first + it->second.size() - index));
                start = it->first;
            } else if (index <= it->first + it->second.size() && accept_end <= it->first + it->second.size()) {
                return ;
            }

            it = next(it);
        }

        std::string new_data = data.substr(0, accept_end - index); 

        if(it != _data_map.end() && accept_end >= it->first && start <= it->first) { // 可与后一项合并
            string add_data = "";
            while(it != _data_map.end() && it->first <= accept_end) {
                if(accept_end - it->first < it->second.size() && it->first + it->second.size() > accept_end) {
                    add_data += it->second.substr(accept_end - it->first, it->second.size() - (accept_end - it->first));
                    accept_end = it->first + it->second.size();
                }
                it = _data_map.erase(it);
            }

            if(can_merge_with_prev) {
                it_prev->second += add_data;

            } else {
                _data_map[index] = new_data + add_data;
            }

        } else if(!can_merge_with_prev) {
            _data_map[index] = data.substr(0, accept_end - index);
        }
         


    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t total_cnt = 0;
    for(auto it : _data_map) {
        total_cnt += it.second.size();
    }

    return total_cnt;
}

bool StreamReassembler::empty() const { return _data_map.empty(); }
