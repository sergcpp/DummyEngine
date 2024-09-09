#pragma once

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <list>

namespace Net {
    struct PacketData {
        unsigned sequence;
        float time;
        int size;

        PacketData(unsigned s, float t, int _size) : sequence(s), time(t), size(_size) {}
    };

    inline bool sequence_more_recent(unsigned int s1, unsigned int s2, unsigned int max_sequence) {
        return ((s1 > s2) && (s1 - s2 <= max_sequence / 2)) || ((s2 > s1) && (s2 - s1 > max_sequence / 2));
    }

    class PacketQueue : public std::list<PacketData> {
    public:
        bool exists(unsigned int sequence) {
            for (auto it = begin(); it != end(); it++) {
                if (it->sequence == sequence) {
                    return true;
                }
            }
            return false;
        }

        void insert_sorted(const PacketData &p, unsigned int max_sequence) {
            if (empty()) {
                push_back(p);
            } else {
                if (!sequence_more_recent(p.sequence, front().sequence, max_sequence)) {
                    push_front(p);
                } else if (sequence_more_recent(p.sequence, back().sequence, max_sequence)) {
                    push_back(p);
                } else {
                    for (auto it = begin(); it != end(); it++) {
                        assert(it->sequence != p.sequence);
                        if (sequence_more_recent(it->sequence, p.sequence, max_sequence)) {
                            insert(it, p);
                            break;
                        }
                    }
                }
            }
        }

        bool verify_sorted(unsigned int max_sequence) {
            auto prev = end();
            for (auto it = begin(); it != end(); it++) {
                //assert(it->sequence <= max_sequence);
                if (it->sequence > max_sequence) {
                    return false;
                }
                if (prev != end()) {
                    //assert(sequence_more_recent(it->sequence, prev->sequence, max_sequence));
                    if (!sequence_more_recent(it->sequence, prev->sequence, max_sequence)) {
                        return false;
                    }
                    prev = it;
                }
            }
            return true;
        }
    };
}

