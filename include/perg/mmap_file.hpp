#pragma once
#include <string>
#include <string_view>

namespace Perg {
    class MmapFile {
    public:
        MmapFile(const std::string& filename);
        ~MmapFile();

        std::string_view view() const;
        size_t size() const;

    private:
        int fd_ = -1;
        void* data_ = nullptr;
        size_t size_ = 0;
    };
}