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

#ifndef _MAPPED_FILE_
#define _MAPPED_FILE_

#include <fcntl.h>
#include <memory>
#include <stdlib.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

class SafeArray {
protected:
    uint8_t *m_buffer;
    size_t m_size;

    SafeArray(uint8_t *buffer, size_t size) : m_buffer(buffer), m_size(size) {
    }

public:
    size_t size() const {
        return m_size;
    }

    uint8_t *buffer() const {
        return m_buffer;
    }

    static std::shared_ptr<SafeArray> create(uint8_t *buffer, size_t size) {
        return std::shared_ptr<SafeArray>(new SafeArray(buffer, size));
    }

    static std::shared_ptr<SafeArray> create(std::vector<uint8_t> &data) {
        return create(data.data(), data.size());
    }

    template <typename T> T *get(size_t offset) const {
        if (offset + sizeof(T) > m_size) {
            return nullptr;
        }

        return reinterpret_cast<T *>(m_buffer + offset);
    }

    uint8_t *get(size_t offset, size_t size) const {
        if (offset + size > m_size) {
            return nullptr;
        }

        return m_buffer + offset;
    }
};

class MappedFile : public SafeArray {
private:
    int m_fd;

    MappedFile(int fd, uint8_t *buffer, size_t size) : m_fd(fd), SafeArray(buffer, size) {
    }

public:
    ~MappedFile() {
        if (m_buffer != MAP_FAILED) {
            munmap(m_buffer, m_size);
        }

        if (m_fd != -1) {
            close(m_fd);
        }
    }

    static std::shared_ptr<MappedFile> create(const std::string &filePath) {
        int fd = open(filePath.c_str(), O_RDONLY);
        if (fd == -1) {
            perror("Error opening file");
            return nullptr;
        }

        struct stat file_stat;
        if (fstat(fd, &file_stat) == -1) {
            perror("Error getting file size");
            close(fd);
            return nullptr;
        }

        size_t size = file_stat.st_size;
        uint8_t *buffer = static_cast<uint8_t *>(mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0));
        if (buffer == MAP_FAILED) {
            perror("Error mapping file to memory");
            close(fd);
            return nullptr;
        }

        return std::shared_ptr<MappedFile>(new MappedFile(fd, buffer, size));
    }
};

#endif