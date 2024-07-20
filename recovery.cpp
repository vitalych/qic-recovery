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

#include <algorithm>
#include <stdio.h>
#include <unordered_map>
#include <vector>
#include "main.h"
#include "qic.h"

static bool check_sig(const SafeArray *file_data, size_t offset, uint32_t sig) {
    auto dat_sig = file_data->get<uint32_t>(offset);
    if (!dat_sig) {
        return false;
    }

    return *dat_sig == sig;
}

static std::string get_native_path(uint16_t *data, size_t char_count) {
    std::vector<uint16_t> path;
    for (int i = 0; i < char_count; ++i) {
        if (data[i] < ' ') {
            path.push_back('/');
        } else {
            path.push_back(data[i]);
        }
    }

    return utf16_to_utf8(path.data(), char_count * sizeof(*data));
}

bool recover_files(const SafeArray *file_data, std::vector<recovered_file_entry_t> &recovered_files) {
    uint32_t dat_sig = DAT_SIG;
    auto occurrences =
        search_binary_substring(file_data->buffer(), file_data->size(), (uint8_t *) &dat_sig, sizeof(uint32_t));
    printf("Found %d occurrences in data of size=%d\n", occurrences.size(), file_data->size());

    for (auto i = 0; i < occurrences.size(); ++i) {
        auto offset = occurrences[i];

        if (!check_sig(file_data, offset, DAT_SIG)) {
            return false;
        }

        offset += sizeof(uint32_t);

        parsed_dir_entry_t dir_entry;
        if (!read_dir_entry(file_data, offset, dir_entry)) {
            fprintf(stderr, "Could not read directory entry");
            return false;
        }

        if (dir_entry.is_dir) {
            continue;
        }

        if (!check_sig(file_data, offset + dir_entry.path_len, EDAT_SIG)) {
            continue;
        }

        // We have a file with high probability, attempt recovery.
        if (dir_entry.path_len > 0) {
            auto path_ptr = file_data->get(offset, dir_entry.path_len);
            if (!path_ptr) {
                continue;
            }

            dir_entry.qic_path = get_native_path((uint16_t *) path_ptr, dir_entry.path_len / 2);
            offset += dir_entry.path_len;
        }

        // Skip EDAT_SIG and the following word.
        offset += sizeof(uint32_t) + 2;

        recovered_file_entry_t entry;
        entry.path = dir_entry.get_native_path();
        entry.offset = offset;
        entry.has_guessed_size = false;
        entry.guessed_size = 0;
        entry.mtime = dir_entry.mtime;
        entry.atime = dir_entry.atime;

        if (i < occurrences.size() - 1) {
            auto next_offset = occurrences[i + 1];
            if (check_sig(file_data, next_offset, DAT_SIG)) {
                entry.guessed_size = next_offset - offset;
                entry.has_guessed_size = true;
            }
        }

        recovered_files.push_back(entry);
    }

    return true;
}

bool extract_file(const SafeArray *file_data, const recovered_file_entry_t *entry) {
    auto buffer = file_data->get(entry->offset, entry->guessed_size);
    if (!buffer) {
        return false;
    }

    std::stringstream path;
    path << "." << entry->path;

    if (entry->may_be_corrupted) {
        path << " [CORRUPTED]";
    }

    auto path_str = path.str();

    fs::path fspath(path_str);
    fs::path dir_path = fspath.parent_path();

    if (!create_dir_tree(dir_path)) {
        return false;
    }

    auto fp = fopen(path_str.c_str(), "wb");
    if (!fp) {
        return false;
    }

    if (fwrite(buffer, entry->guessed_size, 1, fp) != 1) {
        return false;
    }

    fclose(fp);

    if (!update_timestamps(path_str.c_str(), &entry->mtime, &entry->atime)) {
        fprintf(stderr, "Could not update times for %s\n", path_str.c_str());
        return false;
    }

    return true;
}

bool update_times_for_dirs(const std::vector<parsed_dir_entry_t> &parsed_entries) {
    std::unordered_map<std::string, parsed_dir_entry_t> by_path;
    std::vector<std::string> sorted_paths;
    for (const auto &entry : parsed_entries) {
        if (!entry.is_dir) {
            continue;
        }
        auto path = entry.get_recursive_path();
        by_path[path] = entry;
        sorted_paths.push_back(path);
    }

    // Sort the paths, deepest ones come first.
    // This is required so that the attributes of the top most folder is updated
    // after that of the inner most folders. Otherwise the modification
    // date/time of the restored top most folders will be wrong.
    std::sort(sorted_paths.begin(), sorted_paths.end(), [](const std::string &a, const std::string &b) {
        auto ca = std::count(a.begin(), a.end(), '/');
        auto cb = std::count(b.begin(), b.end(), '/');
        if (ca != cb) {
            return ca > cb;
        }
        return a > b;
    });

    for (const auto &path : sorted_paths) {
        const auto &entry = by_path[path];

        std::stringstream local_path;
        local_path << "." << path;
        auto path_str = local_path.str();

        fs::path fspath(path_str);
        if (!create_dir_tree(fspath)) {
            fprintf(stderr, "Could not create %s\n", path_str.c_str());
            continue;
        }

        printf("Updating times for %s\n", path_str.c_str());
        if (!update_timestamps(path_str.c_str(), &entry.mtime, &entry.atime)) {
            fprintf(stderr, "Could not update times for %s\n", path_str.c_str());
            continue;
        }
    }

    return true;
}