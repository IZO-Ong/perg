#pragma once
#include <stdexcept>
#include <string>

namespace Perg {
    class PergException : public std::runtime_error {
    public:
        using std::runtime_error::runtime_error;
    };

    class FileError : public PergException {
    public:
        explicit FileError(const std::string& msg) : PergException("File Error: " + msg) {}
    };

    class RegexError : public PergException {
    public:
        explicit RegexError(const std::string& msg) : PergException("Regex Error: " + msg) {}
    };
}