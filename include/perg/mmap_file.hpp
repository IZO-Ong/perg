#pragma once
#include <string>
#include <string_view>

namespace Perg {

/**
 * @class MmapFile
 * @brief Manages a read-only memory-mapped view of a file.
 * * Provides an efficient way to access file contents by mapping them into 
 * the process's virtual address space, avoiding unnecessary data copies.
 */
class MmapFile {
public:
    /**
     * @brief Maps the specified file into memory.
     * @param filename Path to the target file.
     * @throws FileError If the file cannot be opened or mapping fails.
     */
    MmapFile(const std::string& filename);

    /**
     * @brief Cleans up mapping and closes associated file descriptors.
     */
    ~MmapFile();

    /**
     * @brief Returns a string_view of the entire mapped file.
     * @note The view becomes invalid once the MmapFile object is destroyed.
     */
    std::string_view view() const;

    /**
     * @brief Returns the total size of the mapped file in bytes.
     */
    size_t size() const;

private:
    int fd_ = -1;
    void* data_ = nullptr;
    size_t size_ = 0;
};

} // namespace Perg