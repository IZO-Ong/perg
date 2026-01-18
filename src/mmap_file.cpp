#include "perg/mmap_file.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdexcept>

namespace Perg {
    MmapFile::MmapFile(const std::string& filename) {
        fd_ = open(filename.c_str(), O_RDONLY);
        if (fd_ == -1) { 
            throw std::runtime_error("Could not open file");
        }

        struct stat sb;
        if (fstat(fd_, &sb) == -1) { 
            throw std::runtime_error("Could not get file size");
        }

        size_ = sb.st_size;

        data_ = mmap(NULL, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
        if (data_ == MAP_FAILED) {
            throw std::runtime_error("Mmap failed");
        }
    }

    MmapFile::~MmapFile() {
        if (data_ != MAP_FAILED) munmap(data_, size_);
        if (fd_ != -1) close(fd_);
    }

    std::string_view MmapFile::view() const {
        return std::string_view(static_cast<const char*>(data_), size_);
    }

    size_t MmapFile::size() const {
        return size_;
    }
}