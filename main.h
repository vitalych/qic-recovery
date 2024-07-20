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

#ifndef _MAIN_H_

#define _MAIN_H_

#include <algorithm>
#include <filesystem>
#include <inttypes.h>
#include <unordered_map>
#include <vector>

#include "mapped_file.h"

namespace fs = std::filesystem;

struct parsed_dir_entry_t {
    std::string long_name;
    std::string short_name;
    std::string qic_path;
    bool is_dir;
    bool is_empty_dir;
    bool is_last_entry;
    bool is_dir_end;
    parsed_dir_entry_t *parent;

    size_t dir1_offset;
    size_t dir_data_length;

    size_t path_len;
    size_t file_size;

    struct tm mtime;
    struct tm atime;

    std::string get_recursive_path() const {
        std::vector<std::string> items;
        for (auto current = this; current; current = current->parent) {
            items.push_back(current->long_name);
        }

        std::stringstream ss;
        auto i = items.size();
        for (auto it = items.rbegin(); it != items.rend(); ++it, i--) {
            ss << "/" << *it;
        }

        return ss.str();
    }

    std::string get_native_path() const {
        std::stringstream ss;

        auto native_path = qic_path;
        std::replace(native_path.begin(), native_path.end(), '\n', '/');

        ss << "/" << native_path << "/" << long_name;
        return ss.str();
    }
};

struct recovered_file_entry_t {
    std::string path;
    size_t offset = 0;
    bool has_guessed_size = false;
    size_t guessed_size = 0;
    bool may_be_corrupted = false;

    struct tm mtime = {0};
    struct tm atime = {0};
};

using mdid_t = std::unordered_map<std::string, std::string>;
mdid_t get_mdid(const MappedFile *f, size_t mdid_offset);

bool decompress(const SafeArray *in, std::vector<uint8_t> &out);

bool read_catalog(const MappedFile *file, size_t start_offset, size_t size, std::vector<uint8_t> &buffer);
bool read_data_segment(const MappedFile *file, size_t start_offset, std::vector<uint8_t> &buffer);

bool read_dir_entry(const SafeArray *buffer, size_t &offset, parsed_dir_entry_t &entry);
bool read_dir_entries(const SafeArray *buffer, std::vector<parsed_dir_entry_t> &dirs);
void reconstruct_tree(std::vector<parsed_dir_entry_t> &dirs);

bool recover_files(const SafeArray *file_data, std::vector<recovered_file_entry_t> &recovered_files);
bool extract_file(const SafeArray *file_data, const recovered_file_entry_t *entry);
bool update_times_for_dirs(const std::vector<parsed_dir_entry_t> &parsed_entries);

std::vector<size_t> search_binary_substring(const uint8_t *haystack, size_t haystack_size, const uint8_t *needle,
                                            size_t needle_size);

std::string utf16_to_utf8(const void *buffer, size_t size_in_bytes);

struct tm get_time(unsigned long date);
bool update_timestamps(const char *filepath, const struct tm *mtime, const struct tm *atime);
bool create_dir_tree(const fs::path &dir_path);

#endif