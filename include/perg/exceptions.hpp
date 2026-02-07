#pragma once
#include <stdexcept>
#include <string>

/**
 * @file exceptions.hpp
 * @brief Custom exception hierarchy for the PERG project.
 */

namespace Perg {

/**
 * @class PergException
 * @brief Base class for all exceptions thrown by the PERG engine.
 * Inheriting from std::runtime_error ensures compatibility with standard 
 * C++ exception handling blocks.
 */
class PergException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @class FileError
 * @brief Thrown when filesystem operations fail.
 * Common causes include missing files, permission denials, or mmap failures.
 */
class FileError : public PergException {
public:
    /**
     * @brief Constructs a FileError with a formatted message.
     * @param msg The specific error details from the OS or filesystem.
     */
    explicit FileError(const std::string& msg) : PergException("File Error: " + msg) {}
};

/**
 * @class RegexError
 * @brief Thrown when an invalid regular expression pattern is provided.
 * This captures errors during regex compilation before the scan begins.
 */
class RegexError : public PergException {
public:
    /**
     * @brief Constructs a RegexError with a formatted message.
     * @param msg The description of the regex syntax failure.
     */
    explicit RegexError(const std::string& msg) : PergException("Regex Error: " + msg) {}
};

} // namespace Perg