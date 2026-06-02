#ifndef HYPERTICKET_RATE_LIMITER_HPP
#define HYPERTICKET_RATE_LIMITER_HPP

#include <cstdint>

#include "../../Domain/include/ServiceUtil.hpp"

namespace hyperticket
{
    // 单连接令牌桶限流器，存放于 TcpConnection 的 context 中。
    // 同一连接的回调始终在同一 IO 线程执行（one loop per thread），故无需加锁。
    //
    // 使用整数定点算法（毫令牌 = token × 1000）彻底消除浮点累积误差：
    //   - tokensMilli  : 当前毫令牌数（实际令牌数 × 1000）
    //   - capacityMilli: 桶容量 = perSec × 1000 毫令牌
    //   - refillPerMs  : 每毫秒补充 perSec 毫令牌（即每秒补充 perSec 个真实令牌）
    //   - 消耗一个真实令牌 = 消耗 1000 毫令牌
    struct RateLimiter
    {
        int64_t tokensMilli;    // 当前毫令牌数
        int64_t capacityMilli;  // 桶容量（毫令牌）
        int64_t refillPerMs;    // 每毫秒补充量（毫令牌/ms = perSec）
        int64_t lastMs;         // 上次更新时间（ms）

        explicit RateLimiter(int perSec)
            : tokensMilli(static_cast<int64_t>(perSec) * 1000)
            , capacityMilli(static_cast<int64_t>(perSec) * 1000)
            , refillPerMs(perSec)
            , lastMs(nowMs()) {}

        // 尝试消费一个令牌；无令牌返回 false（应拒绝该请求）。
        bool allow()
        {
            int64_t now = nowMs();
            tokensMilli += (now - lastMs) * refillPerMs;
            if (tokensMilli > capacityMilli) tokensMilli = capacityMilli;
            lastMs = now;
            if (tokensMilli >= 1000)
            {
                tokensMilli -= 1000;
                return true;
            }
            return false;
        }
    };
} // namespace hyperticket
#endif // HYPERTICKET_RATE_LIMITER_HPP
