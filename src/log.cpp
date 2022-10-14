#include <iostream>
#include <iomanip>
#include "log.h"

class DummyLogStream : public std::ostream {
    class DummyLogBuffer : public std::streambuf {
    public:
        int overflow(int c) override {
            return c;
        }
    } dummyBuffer;
public:
    DummyLogStream() : std::ostream(&dummyBuffer) {}
};

inline static std::ostream &log_(std::ostream &stream, std::string_view module) {
    stream << "[+]" << "[" << std::setw(10) << std::left << module << std::right << "] ";
    return stream;
}

std::ostream &log(std::string_view module) {
    return log_(std::cout, module);
}
