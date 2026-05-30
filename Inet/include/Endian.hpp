#ifndef ENDIAN_HPP
#define ENDIAN_HPP

#include <stdint.h>
#include <endian.h>

namespace shanchuan
{
    namespace Sockets
    {
        // 主机字节序转换为网络字节序              64 32 16  系统自带htobe
        inline uint64_t hostToNetwork64(uint64_t host64) { return htobe64(host64); } // 8字节
        inline uint32_t hostToNetwork32(uint32_t host32) { return htobe32(host32); } // 4字节
        inline uint16_t hostToNetwork16(uint16_t host16) { return htobe16(host16); } // 2字节
        // 网络字节序转换为主机字节序                        系统自带be16toh
        inline uint64_t networkToHost64(uint64_t net64)  { return be64toh(net64);  } // 8字节
        inline uint32_t networkToHost32(uint32_t net32)  { return be32toh(net32);  } // 4字节
        inline uint16_t networkToHost16(uint16_t net16)  { return be16toh(net16);  } // 2字节
    } // namespace Sockets
} // namespace shanchuan
#endif // ENDIAN_HPP