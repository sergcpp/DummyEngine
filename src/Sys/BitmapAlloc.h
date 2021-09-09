#pragma once

#include <cassert>
#include <climits>
#include <cstdint>

#ifdef __GNUC__
#define force_inline __attribute__((always_inline)) inline
#endif
#ifdef _MSC_VER
#define force_inline __forceinline

#include <intrin.h>

#pragma intrinsic(_BitScanForward64)
#pragma intrinsic(_tzcnt_u64)
#endif

namespace Sys {
force_inline bool GetFirstBit(const uint64_t mask, unsigned long *bit_index) {
#ifdef _MSC_VER
    return _BitScanForward64(bit_index, mask);
#else
    const int ret = __builtin_ffsll(mask);
    (*bit_index) = ret - 1;
    return ret != 0;
#endif
}

force_inline int CountTrailingZeroes(const uint64_t mask) {
#ifdef _MSC_VER
    return int(_tzcnt_u64(mask));
#else
    if (mask == 0) {
        return 64;
    }
    return __builtin_ctzll(mask);
#endif
}

class BitmapAllocator {
    int block_size_;
    int block_count_;

    uint8_t *memory_;
    uint64_t *bitmap_;

    static const int BitmapGranularity = sizeof(uint64_t) * CHAR_BIT;

  public:
    BitmapAllocator(const int block_size, const int total_size) {
        block_size_ = block_size;
        block_count_ = (total_size + block_size - 1) / block_size;
        block_count_ = BitmapGranularity * ((block_count_ + BitmapGranularity - 1) / BitmapGranularity);

        // TODO: allocate single block
        memory_ = new uint8_t[block_count_ * size_t(block_size_)];
        bitmap_ = new uint64_t[block_count_ / BitmapGranularity];
        memset(bitmap_, 0xff, sizeof(uint64_t) * (block_count_ / BitmapGranularity));
    }

    ~BitmapAllocator() {
        delete[] memory_;
        for (int i = 0; i < block_count_ / BitmapGranularity; ++i) {
            assert(bitmap_[i] == 0xffffffffffffffff && "Not all allocations freed!");
        }
        delete[] bitmap_;
    }

    BitmapAllocator(const BitmapAllocator &rhs) = delete;
    BitmapAllocator(BitmapAllocator &&rhs) = delete;

    BitmapAllocator &operator=(const BitmapAllocator &rhs) = delete;
    BitmapAllocator &operator=(BitmapAllocator &&rhs) = delete;

    void *Alloc_FirstFit(const int size) {
        const int blocks_required = (size + block_size_ - 1) / block_size_;
        int loc_beg = 0;
        // Skip initial occupied blocks
        while (!bitmap_[loc_beg]) {
            ++loc_beg;
        }
#if 1
        const int loc_lim = (block_count_ - blocks_required + BitmapGranularity - 1) / BitmapGranularity;
        unsigned long bit_beg = 0;
        while (loc_beg < loc_lim) {
            if (GetFirstBit(bitmap_[loc_beg] & ~((1ull << bit_beg) - 1), &bit_beg)) {
                int bit_end = CountTrailingZeroes(~(bitmap_[loc_beg] | ((1ull << bit_beg) - 1)));
                int loc_end = loc_beg;
                if (bit_end == BitmapGranularity) {
                    ++loc_end;
                    bit_end = 0;
                    while (loc_end < (block_count_ / BitmapGranularity) &&
                           (loc_end - loc_beg) * BitmapGranularity - int(bit_beg) + bit_end < blocks_required) {
                        bit_end = CountTrailingZeroes(~bitmap_[loc_end]);
                        if (bit_end != BitmapGranularity) {
                            break;
                        }
                        ++loc_end;
                        bit_end = 0;
                    }
                }

                const int blocks_found = (loc_end - loc_beg) * BitmapGranularity - bit_beg + bit_end;
                if (blocks_found >= blocks_required) {
                    // Mark blocks as occupied
                    const int block_beg = loc_beg * BitmapGranularity + bit_beg;
                    for (int i = block_beg; i < block_beg + blocks_required; ++i) {
                        const int xword_index = i / BitmapGranularity;
                        const int bit_index = i % BitmapGranularity;
                        bitmap_[xword_index] &= ~(1ull << bit_index);
                    }
                    return memory_ + block_size_ * (uintptr_t(loc_beg) * BitmapGranularity + bit_beg);
                }
                bit_beg = bit_end;
                loc_beg = loc_end;
            } else {
                ++loc_beg;
            }
        }
#else
        loc_beg *= BitmapGranularity;
        for (; loc_beg <= block_count_ - blocks_required;) {
            if (!bitmap_[loc_beg / BitmapGranularity]) {
                const int count = (BitmapGranularity - (loc_beg % BitmapGranularity));
                loc_beg += count;
                continue;
            }

            // Count the number of available blocks
            int loc_end = loc_beg;
            while (loc_end < loc_beg + blocks_required) {
                const int xword_index = loc_end / BitmapGranularity;
                const int bit_index = loc_end % BitmapGranularity;
                if ((bitmap_[xword_index] & (1ull << bit_index)) == 0) {
                    break;
                }
                ++loc_end;
            }

            if ((loc_end - loc_beg) >= blocks_required) {
                // Mark blocks as occupied
                for (int i = loc_beg; i < loc_beg + blocks_required; ++i) {
                    const int xword_index = i / BitmapGranularity;
                    const int bit_index = i % BitmapGranularity;
                    bitmap_[xword_index] &= ~(1ull << bit_index);
                }
                return memory_ + uintptr_t(block_size_) * loc_beg;
            } else {
                loc_beg = loc_end + 1;
            }
        }
#endif

        return nullptr;
    }

    void *Alloc_BestFit(const int size) {
        const int blocks_required = (size + block_size_ - 1) / block_size_;

        int best_blocks_available = block_count_ + 1;
        int best_loc = -1;

        int loc_beg = 0;
        // Skip initial occupied blocks
        while (!bitmap_[loc_beg]) {
            ++loc_beg;
        }

#if 1
        const int loc_lim = (block_count_ - blocks_required + BitmapGranularity - 1) / BitmapGranularity;
        unsigned long bit_beg = 0;
        while (loc_beg < loc_lim) {
            if (GetFirstBit(bitmap_[loc_beg] & ~((1ull << bit_beg) - 1), &bit_beg)) {
                int bit_end = CountTrailingZeroes(~(bitmap_[loc_beg] | ((1ull << bit_beg) - 1)));
                int loc_end = loc_beg;
                if (bit_end == BitmapGranularity) {
                    ++loc_end;
                    bit_end = 0;
                    while (loc_end < loc_lim &&
                           (loc_end - loc_beg) * BitmapGranularity - int(bit_beg) + bit_end < blocks_required) {
                        bit_end = CountTrailingZeroes(~bitmap_[loc_end]);
                        if (bit_end != BitmapGranularity) {
                            break;
                        }
                        ++loc_end;
                        bit_end = 0;
                    }
                }

                const int blocks_found = (loc_end - loc_beg) * BitmapGranularity - bit_beg + bit_end;
                if (blocks_found >= blocks_required && blocks_found < best_blocks_available) {
                    best_blocks_available = blocks_found;
                    best_loc = loc_beg * BitmapGranularity + bit_beg;
                    if (blocks_found == blocks_required) {
                        // Perfect fit was found, can stop here
                        break;
                    }
                }
                bit_beg = bit_end;
                loc_beg = loc_end;
            } else {
                ++loc_beg;
            }
        }
#else
        loc_beg *= BitmapGranularity;
        for (; loc_beg <= block_count_ - blocks_required;) {
            if (!bitmap_[loc_beg / BitmapGranularity]) {
                const int count = (BitmapGranularity - (loc_beg % BitmapGranularity));
                loc_beg += count;
                continue;
            }

            // Count the number of available blocks
            int loc_end = loc_beg;
            while (loc_end < block_count_) {
                const int xword_index = loc_end / BitmapGranularity;
                const int bit_index = loc_end % BitmapGranularity;
                if ((bitmap_[xword_index] & (1ull << bit_index)) == 0) {
                    break;
                }
                ++loc_end;
            }

            if ((loc_end - loc_beg) >= blocks_required && (loc_end - loc_beg) < best_blocks_available) {
                best_blocks_available = (loc_end - loc_beg);
                best_loc = loc_beg;
                if ((loc_end - loc_beg) == blocks_required) {
                    // Perfect fit was found, can stop here
                    break;
                }
            }
            loc_beg = loc_end + 1;
        }
#endif

        if (best_loc != -1) {
            // Mark blocks as occupied
            for (int i = best_loc; i < best_loc + blocks_required; ++i) {
                const int xword_index = i / BitmapGranularity;
                const int bit_index = i % BitmapGranularity;
                bitmap_[xword_index] &= ~(1ull << bit_index);
            }
            return memory_ + uintptr_t(block_size_) * best_loc;
        }

        return nullptr;
    }

    void Free(void *p, const int size) {
        if (!p) {
            return;
        }

        const uintptr_t mem_offset = uintptr_t(p) - uintptr_t(memory_);
        assert(mem_offset % block_size_ == 0);
        const int block_index = int(mem_offset / block_size_);
        assert(block_index < block_count_);
        const int blocks_required = (size + block_size_ - 1) / block_size_;

        // Mark blocks as free
        for (int i = block_index; i < block_index + blocks_required; ++i) {
            const int xword_index = i / BitmapGranularity;
            const int bit_index = i % BitmapGranularity;
            assert((bitmap_[xword_index] & (1ull << bit_index)) == 0);
            bitmap_[xword_index] |= (1ull << bit_index);
        }
    }
};
} // namespace Sys

#undef force_inline
