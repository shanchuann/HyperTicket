#ifndef NOTCOPY_HPP
#define NOTCOPY_HPP

namespace shanchuan
{
    struct Notcopy
    {
        Notcopy() {}
        Notcopy(const Notcopy &other) = delete;
        Notcopy &operator=(const Notcopy &other) = delete;
    };
} // namespace shanchuan
#endif // NOTCOPY_HPP