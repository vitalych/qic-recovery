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

#include "main.h"
#include "qic.h"

bool read_catalog(const MappedFile *file, size_t start_offset, size_t size, std::vector<uint8_t> &buffer) {
    while (size > 0) {
        auto seg_head = file->get<cseg_head_t>(start_offset);
        if (!seg_head) {
            return false;
        }

        start_offset += sizeof(cseg_head_t);

        auto frame_head = file->get<cframe_head_t>(start_offset);

        bool compressed = (frame_head->segment_size & RAW_SEG) == 0;
        if (compressed) {
            fprintf(stderr, "Compression not supported\n");
            return false;
        }

        auto segment_size = frame_head->segment_size & ~RAW_SEG;

        start_offset += sizeof(cframe_head_t);
        auto data = file->get(start_offset, segment_size);
        if (!data) {
            return false;
        }

        for (auto i = 0; i < segment_size; ++i) {
            buffer.push_back(data[i]);
        }

        start_offset += segment_size;
        size -= segment_size;
    }

    return true;
}

bool read_data_segment(const MappedFile *file, size_t start_offset, std::vector<uint8_t> &buffer) {
    auto total_read = 0;
    auto segments_read = 0;

    while (true) {
        auto seg_head = file->get<cseg_head_t>(start_offset);
        if (!seg_head) {
            return false;
        }

        start_offset += sizeof(cseg_head_t);

        auto frame_head = file->get<cframe_head_t>(start_offset);

        bool compressed = (frame_head->segment_size & RAW_SEG) == 0;
        auto segment_size = frame_head->segment_size & ~RAW_SEG;
        if (segment_size == 0) {
            break;
        }

        start_offset += sizeof(cframe_head_t);
        auto data = file->get(start_offset, segment_size);
        if (!data) {
            return false;
        }

        if (compressed) {
            auto array = SafeArray::create(data, segment_size);
            if (!array) {
                return false;
            }
            if (!decompress(array.get(), buffer)) {
                fprintf(stderr, "decompression failed\n");
                return false;
            }
        } else {
            for (auto i = 0; i < segment_size; ++i) {
                buffer.push_back(data[i]);
            }
        }

        start_offset += segment_size;
        total_read += segment_size;
        ++segments_read;
    }

    return true;
}
