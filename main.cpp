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

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s /path/to/file.qic\n", argv[0]);
        return -1;
    }

    auto file = MappedFile::create(argv[1]);
    if (!file) {
        fprintf(stderr, "Could not open %s\n", argv[1]);
        return -2;
    }

    auto vtbl = file->get<qic_vtbl_t>(0);
    if (!vtbl) {
        fprintf(stderr, "Could not read vtbl\n");
        return -3;
    }

    auto mdid = get_mdid(file.get(), sizeof(qic_vtbl_t));
    if (mdid.empty()) {
        fprintf(stderr, "Could not read mdid\n");
        return -4;
    }

    auto i = vtbl->dir_size / SEG_SZ;
    if (vtbl->dir_size % SEG_SZ) {
        i++;
    }
    auto dir_offset = file->size() - i * SEG_SZ;

    std::vector<uint8_t> dir_buffer;
    if (!read_catalog(file.get(), dir_offset, vtbl->dir_size, dir_buffer)) {
        fprintf(stderr, "Could not read catalog\n");
        return -5;
    }

    auto dir_data = SafeArray::create(dir_buffer);
    std::vector<parsed_dir_entry_t> parsed_entries;
    if (!read_dir_entries(dir_data.get(), parsed_entries)) {
        fprintf(stderr, "Could not parse dir entries");
        return -6;
    }

    std::unordered_map<std::string, const parsed_dir_entry_t *> m_file_map;
    reconstruct_tree(parsed_entries);
    auto file_count = 0;
    for (const auto &entry : parsed_entries) {
        auto path = entry.get_recursive_path();
        printf("D=%d ED=%d LE=%d LN=%-20s %s\n", entry.is_dir, entry.is_empty_dir, entry.is_last_entry,
               entry.long_name.c_str(), path.c_str());
        if (!entry.is_dir) {
            file_count++;
        }

        m_file_map[path] = &entry;
    }

    // Read compressed file data.
    auto file_data_offset = 0x100;
    std::vector<uint8_t> file_buffer;
    if (!read_data_segment(file.get(), file_data_offset, file_buffer)) {
        fprintf(stderr, "Could not read data segment\n");
        // return -7;
    }

    auto file_data = SafeArray::create(file_buffer);
    std::vector<recovered_file_entry_t> recovered_files;
    recover_files(file_data.get(), recovered_files);

    auto total_size = 0;
    auto error_count = 0;
    for (const auto &file : recovered_files) {
        printf("%s gs=%d size=%d offset=%#x\n", file.path.c_str(), file.has_guessed_size, file.guessed_size,
               file.offset);
        total_size += file.guessed_size;

        auto final_size = file.guessed_size;

        auto it = m_file_map.find(file.path);
        if (it == m_file_map.end()) {
            fprintf(stderr, "Could not find %s in directory catalog\n", file.path.c_str());
            ++error_count;
            continue;
        }

        auto final_entry = file;
        if ((*it).second->file_size != file.guessed_size) {
            fprintf(stderr, "Mismatched file size for %s: catalog: %#x recovered: %#x\n", file.path.c_str(),
                    (*it).second->file_size, file.guessed_size);
            ++error_count;

            if (final_size == 0) {
                final_size = (*it).second->file_size;
            } else {
                final_entry.may_be_corrupted = true;
            }
        }

        final_entry.guessed_size = final_size;
        extract_file(file_data.get(), &final_entry);
    }

    printf("error_count=%d file_count: %d recovered_file_count: %d total_size: %d\n", error_count, file_count,
           recovered_files.size(), total_size);

    update_times_for_dirs(parsed_entries);

    return 0;
}