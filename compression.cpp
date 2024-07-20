///
/// Copyright (C) 2024 Vitaly Chipounov
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in all
/// copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
/// SOFTWARE.
///

#include <inttypes.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory.h>
#include <memory>
#include <stdexcept>
#include <vector>

#include "main.h"
#include "mapped_file.h"

class BitStream {
    const uint8_t *m_buffer;
    size_t m_size;

    // Keeps track of the current bit position.
    size_t m_bit_pos;

public:
    BitStream(const uint8_t *buffer, size_t buffer_size) : m_buffer(buffer), m_size(buffer_size), m_bit_pos(0) {
        if (!buffer) {
            throw std::invalid_argument("Buffer cannot be null");
        }
    }

    bool get_next_bit(uint8_t *bit) {
        if (m_bit_pos >= m_size * 8) {
            return false; // No more bits available
        }

        size_t byte_index = m_bit_pos / 8;
        size_t bit_index = m_bit_pos % 8;

        *bit = (m_buffer[byte_index] >> (7 - bit_index)) & 0x01;
        ++m_bit_pos;

        return true;
    }

    bool get_next_byte(uint8_t *byte) {
        return get_next_bits<uint8_t>(byte, 8);
    }

    template <typename T> bool get_next_bits(T *out, size_t count) {
        if (count > sizeof(T) * 8) {
            return false;
        }

        *out = 0;

        for (int i = 0; i < count; ++i) {
            uint8_t bit;
            if (!get_next_bit(&bit)) {
                return false; // Should not happen, but just in case
            }
            *out = (*out << 1) | bit;
        }

        return true;
    }

    static std::shared_ptr<BitStream> create(const uint8_t *buffer, size_t buffer_size) {
        return std::make_shared<BitStream>(buffer, buffer_size);
    }
};

static bool get_offset(BitStream *stream, uint16_t *offset) {
    uint8_t is_7bit;
    if (!stream->get_next_bit(&is_7bit)) {
        return false;
    }

    if (is_7bit) {
        return stream->get_next_bits(offset, 7);
    } else {
        return stream->get_next_bits(offset, 11);
    }
}

static bool get_length(BitStream *stream, uint32_t *length) {
    uint8_t nibble;

    *length = 0;

    for (auto i = 0; i < 2; i++) {
        nibble = 0;
        if (!stream->get_next_bits(&nibble, 2)) {
            return false;
        }

        if (nibble < 3) {
            *length += nibble + 2;
            return true;
        } else {
            *length += 3;
        }
    }

    while (true) {
        nibble = 0;
        if (!stream->get_next_bits(&nibble, 4)) {
            return false;
        }

        if (nibble < 15) {
            *length += nibble + 2;
            return true;
        } else {
            *length += 15;
        }
    }

    return true;
}

class HistoryBuffer {
    std::vector<uint8_t> m_history;
    std::vector<uint8_t> &m_out;
    size_t m_offset;

public:
    HistoryBuffer(std::vector<uint8_t> &out) : m_history(), m_out(out), m_offset(0) {
        m_history.resize(2048);
    }

    ~HistoryBuffer() {
        flush();
    }

    void flush() {
        for (auto i = 0; i < m_offset; ++i) {
            m_out.push_back(m_history[i]);
        }
        m_offset = 0;
    }

    void put(uint8_t byte) {
        if (m_offset == m_history.size()) {
            flush();
        }
        m_history[m_offset++] = byte;
    }

    void put(const size_t offset, size_t length) {
        while (length-- > 0) {
            if (m_offset == m_history.size()) {
                flush();
            }

            size_t index = 0;
            if (m_offset >= offset) {
                index = m_offset - offset;
            } else {
                index = m_offset + 2048 - offset;
            }
            m_history[m_offset++] = m_history[index % 2048];
        }
    }
};

bool decompress(const SafeArray *in, std::vector<uint8_t> &out) {
    HistoryBuffer history(out);

    auto buffer = in->get(0, in->size());
    if (!buffer) {
        return false;
    }

    auto stream = BitStream::create(buffer, in->size());
    if (!stream) {
        return false;
    }

    while (true) {
        uint8_t byte;
        uint8_t is_compressed;
        if (!stream->get_next_bit(&is_compressed)) {
            return false;
        }

        if (is_compressed) {
            uint16_t offset;
            if (!get_offset(stream.get(), &offset)) {
                return false;
            }

            if (offset == 0) {
                // Finished decompressing.
                return true;
            }

            uint32_t length;
            if (!get_length(stream.get(), &length)) {
                return false;
            }

            history.put(offset, length);
        } else {
            if (!stream->get_next_byte(&byte)) {
                return false;
            }

            history.put(byte);
        }
    }

    return true;
}
