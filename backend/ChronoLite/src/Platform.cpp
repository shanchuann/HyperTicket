#include "Platform.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace logsys {
std::string hostname() {
    char buffer[256] = {};
#ifdef _WIN32
    DWORD size = static_cast<DWORD>(sizeof(buffer));
    return GetComputerNameA(buffer, &size) ? std::string(buffer, size) : "unknownhost";
#else
    return ::gethostname(buffer, sizeof(buffer)) == 0 ? std::string(buffer) : "unknownhost";
#endif
}

std::uint64_t processId() {
#ifdef _WIN32
    return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}
}
