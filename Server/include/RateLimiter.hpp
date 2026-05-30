#ifndef HYPERTICKET_RATE_LIMITER_HPP
#define HYPERTICKET_RATE_LIMITER_HPP

#include <cstdint>

#include "../../Domain/include/ServiceUtil.hpp"

namespace hyperticket
{
    // 单连接令牌桶限流器，存放于 TcpConnection 的 context 中。
    // 同一连接的回调始终在同一 IO 线程执行（one loop per thread），故无需加锁。
    struct RateLimiter
    {
        double tokens;      // 当前令牌数
        double capacity;    // 桶容量 = 每秒最大请求数
        double refillPerMs; // 每毫秒补充的令牌
        int64_t lastMs;     // 上次补充时间

        explicit RateLimiter(int perSec)
            : tokens(perSec), capacity(perSec),
              refillPerMs(perSec / 1000.0), lastMs(nowMs()) {}

        // 尝试消费一个令牌；无令牌返回 false（应拒绝该请求）。
        bool allow()
        {
            int64_t now = nowMs();
            tokens += (now - lastMs) * refillPerMs;
            if (tokens > capacity) tokens = capacity;
            lastMs = now;
            if (tokens >= 1.0)
            {
                tokens -= 1.0;
                return true;
            }
            return false;
        }
    };
} // namespace hyperticket
#endif // HYPERTICKET_RATE_LIMITER_HPP
