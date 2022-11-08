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

inline static std::ostream &log_(std::ostream &out, std::string_view module, bool critical) {
    char leading = critical ? '!' : '+';
    out << "[" << leading << "] [" << std::setw(10) << std::left << module << std::right << "] ";
    return out;
}


std::ostream &log(std::string_view module) {
#ifdef CONF_LOG_OUTPUT
    return log_(std::cout, module, false);
#else
    static DummyLogStream dummy;
    return dummy;
#endif
}

std::ostream &err(std::string_view module) {
    return log_(std::cout, module, true);
}
