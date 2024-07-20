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
#include <filesystem>
#include <functional>
#include <inttypes.h>
#include <iostream>
#include <string>
#include <utime.h>
#include <vector>

#include "main.h"

namespace fs = std::filesystem;

bool create_dir_tree(const fs::path &dir_path) {
    if (dir_path.empty()) {
        return false;
    }

    if (fs::exists(dir_path)) {
        return true;
    }

    std::error_code ec;
    if (!fs::create_directories(dir_path, ec)) {
        if (ec) {
            std::cerr << "Error creating directory tree: " << ec.message() << std::endl;
            return false;
        }
    }

    return true;
}

std::vector<size_t> search_binary_substring(const uint8_t *haystack, size_t haystack_size, const uint8_t *needle,
                                            size_t needle_size) {
    std::vector<size_t> occurrences;
    if (needle_size == 0 || haystack_size < needle_size) {
        return occurrences;
    }

    auto searcher = std::boyer_moore_searcher(needle, needle + needle_size);
    const uint8_t *end = haystack + haystack_size;

    auto it = haystack;
    while (it != end) {
        it = std::search(it, end, searcher);
        if (it != end) {
            occurrences.push_back(it - haystack);
            ++it;
        }
    }

    return occurrences;
}

std::string utf16_to_utf8(const void *buffer, size_t size_in_bytes) {
    if (size_in_bytes % 2 != 0) {
        return "";
    }

    auto utf16_string = static_cast<const char16_t *>(buffer);
    size_t utf16_length = size_in_bytes / 2;

    try {
        std::u16string utf16_str(utf16_string, utf16_length);
        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
        std::string utf8_str = convert.to_bytes(utf16_str);
        return utf8_str;
    } catch (...) {
        return "";
    }
}

static const uint16_t BASEYR = 1970;

struct tm get_time(unsigned long date) {
    struct tm ret = {0};
    uint8_t mondays[] = {31, 28, 31, 30, 31, 30, 31, 31, 31, 30, 31, 31};
    uint16_t yr = BASEYR, mon = 0, day, hour, min, sec;
    char lpyr;
    sec = date % 60;
    date /= 60;
    min = date % 60;
    date /= 60;
    hour = date % 24;
    date /= 24;
    do {
        if ((yr % 100) == 0 || (yr % 4) != 0) {
            day = 365; // not a leap year
            lpyr = 0;
        } else {
            day = 366; // is a leap year
            lpyr = 1;
        }
        if (date > day) {
            yr++;
            date -= day;
        }
    } while (date > day);
    day = date;

    mondays[1] += lpyr;
    while (mon < 12) {
        if (mondays[mon] >= day) {
            break;
        } else {
            day -= mondays[mon++];
        }
    }
    mondays[1] -= lpyr;

    ret.tm_mday = day;
    ret.tm_mon = mon + 1;
    ret.tm_year = yr - 1900;
    ret.tm_hour = hour;
    ret.tm_min = min;
    ret.tm_sec = sec;

    return ret;
}

bool update_timestamps(const char *filepath, const struct tm *mtime, const struct tm *atime) {
    struct utimbuf new_times = {0};

    auto m = *mtime;
    auto a = *atime;
    new_times.actime = mktime(&a);
    new_times.modtime = mktime(&m);
    if (utime(filepath, &new_times) < 0) {
        return false;
    }

    return true;
}
