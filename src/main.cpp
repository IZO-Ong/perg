#include <iostream>
#include <string>
#include "perg/mmap_file.hpp"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: perg <pattern> <file>" << std::endl;
        return 1;
    }

    std::string pattern = argv[1];
    std::string filename = argv[2];

    try {
        Perg::MmapFile file(filename);
        std::string_view content = file.view();

        size_t pos = content.find(pattern);
        int count = 0;

        while (pos != std::string_view::npos) {
            count++;
            pos = content.find(pattern, pos + pattern.size());
        }

        std::cout << "Found '" << pattern << "' " << count << " times." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}