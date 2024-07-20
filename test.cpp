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

#include <cassert>
#include "main.h"

static void test_decompress() {
    uint8_t compressed[] = {0x20, 0x90, 0x88, 0x38, 0x1C, 0x21, 0xE2, 0x5C, 0x15, 0x80};

    auto array = SafeArray::create(compressed, sizeof(compressed));

    std::vector<uint8_t> decompressed;
    if (!decompress(array.get(), decompressed)) {
        fprintf(stderr, "Failed decompression");
        return;
    }

    assert(decompressed.size() == 16);
}

/*           dir last_dir
COMEXE       1 0
config.sys   0 0
TEXT         1 1
STUFF        1 0
LANGUAGE     1 1
stuff.dat    0 1
APL          1 0
C            1 0
BASIC        1 1
hello.c      0 1
mortgage.bas 0 1
readme.txt   0 1
*/

static void test(void) {
    // clang-format off
    std::vector<parsed_dir_entry_t> entries = {
        {.long_name="", .short_name="", .is_dir=true, .is_empty_dir=false, .is_last_entry=true, .parent=nullptr},
        {.long_name="COMEXE", .short_name="COMEXE", .is_dir=true, .is_empty_dir=false, .is_last_entry=false, .parent=nullptr},
        {.long_name="config.sys", .short_name="config.sys", .is_dir=false, .is_empty_dir=false, .is_last_entry=false, .parent=nullptr},
        {.long_name="TEXT", .short_name="TEXT", .is_dir=true, .is_empty_dir=false, .is_last_entry=true, .parent=nullptr},
        {.long_name="STUFF", .short_name="STUFF", .is_dir=true, .is_empty_dir=false, .is_last_entry=false, .parent=nullptr},
        {.long_name="LANGUAGE", .short_name="LANGUAGE", .is_dir=true, .is_empty_dir=false, .is_last_entry=true, .parent=nullptr},
        {.long_name="stuff.dat", .short_name="stuff.dat", .is_dir=false, .is_empty_dir=false, .is_last_entry=true, .parent=nullptr},
        {.long_name="APL", .short_name="APL", .is_dir=true, .is_empty_dir=true, .is_last_entry=false, .parent=nullptr},
        {.long_name="C", .short_name="C", .is_dir=true, .is_empty_dir=false, .is_last_entry=false, .parent=nullptr},
        {.long_name="BASIC", .short_name="BASIC", .is_dir=true, .is_empty_dir=false, .is_last_entry=true, .parent=nullptr},
        {.long_name="hello.c", .short_name="hello.c", .is_dir=false, .is_empty_dir=false, .is_last_entry=true, .parent=nullptr},
        {.long_name="mortgage.bas", .short_name="mortgage.bas", .is_dir=false, .is_empty_dir=false, .is_last_entry=true, .parent=nullptr},
        {.long_name="readme.txt", .short_name="readme.txt", .is_dir=false, .is_empty_dir=false, .is_last_entry=true, .parent=nullptr}
    };
    // clang-format on

    reconstruct_tree(entries);

    for (const auto &entry : entries) {
        auto path = entry.get_recursive_path();
        printf("D=%d ED=%d LE=%d LN=%-20s %s\n", entry.is_dir, entry.is_empty_dir, entry.is_last_entry,
               entry.long_name.c_str(), path.c_str());
    }

    assert(entries[0].parent == nullptr);
    assert(entries[1].parent == &entries[0]);
    assert(entries[2].parent == &entries[0]);
    assert(entries[3].parent == &entries[0]);
    assert(entries[4].parent == &entries[1]);
    assert(entries[5].parent == &entries[1]);
    assert(entries[6].parent == &entries[4]);
    assert(entries[7].parent == &entries[5]);
    assert(entries[8].parent == &entries[5]);
    assert(entries[9].parent == &entries[5]);
    assert(entries[10].parent == &entries[8]);
    assert(entries[11].parent == &entries[9]);
    assert(entries[12].parent == &entries[3]);
}

int main(int argc, char **argv) {
    test();
    test_decompress();
}
