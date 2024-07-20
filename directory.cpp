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

#include <deque>
#include <memory>
#include <stack>
#include <vector>
#include "main.h"
#include "qic.h"

bool read_dir_entry(const SafeArray *buffer, size_t &offset, parsed_dir_entry_t &entry) {
    entry.dir1_offset = offset;

    auto d1 = buffer->get<ms_dir_fixed_t>(offset);
    if (!d1) {
        return false;
    }

    offset += sizeof(ms_dir_fixed_t);

    if (d1->nm_len > 0) {
        auto name_ptr = buffer->get(offset, d1->nm_len);
        if (!name_ptr) {
            return false;
        }
        entry.long_name = utf16_to_utf8(name_ptr, d1->nm_len);
        offset += d1->nm_len;
    }

    auto d2 = buffer->get<ms_dir_fixed2_t>(offset);
    if (!d2) {
        return false;
    }

    offset += sizeof(ms_dir_fixed2_t);

    auto dos_len = d2->nm_len;
    if (dos_len == 0) {
        dos_len = d1->nm_len;
    }

    if (dos_len > 0) {
        auto name_ptr = buffer->get(offset, dos_len);
        if (!name_ptr) {
            return false;
        }

        entry.short_name = utf16_to_utf8(name_ptr, dos_len);
        offset += dos_len;
    }

    entry.is_dir = d1->flag & SUBDIR;
    entry.is_empty_dir = d1->flag & EMPTYDIR;
    entry.is_last_entry = d1->flag & DIRLAST;
    entry.is_dir_end = d1->flag & DIREND;
    entry.dir_data_length = offset - entry.dir1_offset;
    entry.path_len = d1->path_len;
    entry.file_size = d1->file_len;
    entry.mtime = get_time(d1->m_datetime);
    entry.atime = get_time(d1->a_datetime);

    return true;
}

bool read_dir_entries(const SafeArray *buffer, std::vector<parsed_dir_entry_t> &dirs) {
    size_t offset = 0;

    while (true) {
        parsed_dir_entry_t entry;

        if (!read_dir_entry(buffer, offset, entry)) {
            return false;
        }

        dirs.push_back(entry);

        if (entry.is_dir_end) {
            break;
        }
    }

    return true;
}

template <typename T> using vector_ptr = std::shared_ptr<std::deque<T>>;

void reconstruct_tree(std::vector<parsed_dir_entry_t> &dirs) {
    parsed_dir_entry_t *current_parent = nullptr;

    std::stack<vector_ptr<parsed_dir_entry_t *>> stack;

    auto current_level = std::make_shared<std::deque<parsed_dir_entry_t *>>();
    current_level->push_back(nullptr);
    stack.push(current_level);

    bool first = true;

    for (int i = 0; i < dirs.size(); ++i) {
        if (first) {
            first = false;
            auto top = stack.top();
            current_parent = top->front();
            top->pop_front();
            if (top->empty()) {
                stack.pop();
            }

            stack.push(std::make_shared<std::deque<parsed_dir_entry_t *>>());
        }

        auto current_entry = &dirs[i];
        current_entry->parent = current_parent;

        auto has_children = current_entry->is_dir && !current_entry->is_empty_dir;
        if (has_children) {
            stack.top()->push_back(current_entry);
        }

        if (current_entry->is_last_entry) {
            first = true;
            if (stack.top()->size() == 0) {
                stack.pop();
            }
        }
    }
}
