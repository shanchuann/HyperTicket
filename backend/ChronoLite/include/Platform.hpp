#ifndef LOGSYS_PLATFORM_HPP
#define LOGSYS_PLATFORM_HPP

#include <cstdint>
#include <string>

namespace logsys {
std::string hostname();
std::uint64_t processId();
}

#endif
