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

#include <cstring>
#include "main.h"

static const uint8_t MDID_TERM = 0xb0;
static const size_t VTBL_SZ = 128;
static const char *MDID_IDs[] = {"MediumID", "VR", "CS", "FM", "UL", "DT", NULL};

static std::vector<std::string> split(const char *data, size_t size, char separator) {
    std::vector<std::string> result;
    std::string currentString;

    for (size_t i = 0; i < size; ++i) {
        if (data[i] == separator) {
            if (!currentString.empty()) {
                result.push_back(currentString);
                currentString.clear();
            }
        } else if (data[i]) {
            currentString += data[i];
        } else {
            break;
        }
    }

    // Add the last string if it's not empty
    if (!currentString.empty()) {
        result.push_back(currentString);
    }

    return result;
}

mdid_t get_mdid(const MappedFile *f, size_t mdid_offset) {
    mdid_t ret;

    if (!f->get<uint32_t>(mdid_offset)) {
        return {};
    }

    auto strptr = f->get(mdid_offset + 4, VTBL_SZ - 4);
    if (!strptr) {
        return {};
    }

    auto strings = split((const char *) strptr, VTBL_SZ - 4, MDID_TERM);
    for (const auto &str : strings) {
        const char *medium_id = "MediumID";
        if (str.rfind(medium_id, 0) == 0) {
            ret[medium_id] = str.substr(strlen(medium_id));
        } else {
            // Assume the id is two chars big
            ret[str.substr(0, 2)] = str.substr(2);
        }
    }

    return ret;
}
