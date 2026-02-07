#include "perg/exceptions.hpp"
#include "perg/mmap_file.hpp"

#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>

namespace Perg {

/**
 * @brief Constructs an MmapFile object by mapping a file into the process's virtual address space.
 * This constructor implements RAII for low-level system resources (file descriptors and memory mappings).
 * It uses the private mapping (MAP_PRIVATE) mode as the scanner only requires read access.
 * @param filename The filesystem path of the file to be mapped.
 * @throws Perg::FileError If the file cannot be opened, sized, or mapped.
 */
MmapFile::MmapFile(const std::string& filename) {
    fd_ = open(filename.c_str(), O_RDONLY);
    if (fd_ == -1) { 
        throw Perg::FileError("Could not open file: " + filename);
    }

    struct stat sb;
    if (fstat(fd_, &sb) == -1) { 
        close(fd_);
        throw Perg::FileError("Could not get file size for: " + filename);
    }

    size_ = sb.st_size;

    // Handle empty files, as mmap size must be > 0
    if (size_ == 0) {
        data_ = nullptr;
        return;
    }

    data_ = mmap(NULL, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
    if (data_ == MAP_FAILED) {
        close(fd_);
        throw Perg::FileError("Mmap failed for: " + filename);
    }

    /** Optimizes sequential access performance.
     *  MADV_SEQUENTIAL: Signals the kernel to aggressiveley pre-fetch data.
     *  MADV_WILLNEED: Advises the kernel that we intend to access the entire range soon.
     */
    madvise(data_, size_, MADV_SEQUENTIAL | MADV_WILLNEED);
}

/**
 * @brief Destructor ensures that memory is unmapped and file descriptors are closed.
 * The order of operations is critical to prevent resource leaks in the event of
 * partial object initialization or system-level failures.
 */
MmapFile::~MmapFile() {
    if (data_ != nullptr && data_ != MAP_FAILED) {
        munmap(data_, size_);
    }
    if (fd_ != -1) {
        close(fd_);
    }
}

/**
 * @brief Provides a non-owning view of the mapped file data.
 * @return A std::string_view pointing to the memory-mapped region.
 */
std::string_view MmapFile::view() const {
    if (size_ == 0) return std::string_view();
    return std::string_view(static_cast<const char*>(data_), size_);
}

/**
 * @brief Returns the size of the mapped file in bytes.
 */
size_t MmapFile::size() const {
    return size_;
}

} // namespace Perg