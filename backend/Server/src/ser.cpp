// HyperTicket 服务端入口：仅负责组装（网络 + 线程池 + 调度 + 限流），
// 业务逻辑下沉到 Domain/service/TicketService，协议下沉到 Common/Protocol。
#include "../../Common/include/AppConfig.hpp"
#include "../../Common/include/Protocol.hpp"
#include "../../Common/include/Errors.hpp"
#include "../include/SessionManager.hpp"
#include "../include/RedisSessionManager.hpp"
#include "../include/RedisConnPool.hpp"
#include "../include/RedisStockCache.hpp"
#include "../include/MetricsManager.hpp"
#include "../include/RateLimiter.hpp"
#include "../include/SchemaInitializer.hpp"
#include "../../Domain/include/service/TicketService.hpp"
#include "../../Domain/include/ServiceUtil.hpp"
#include "../../SqlConnPool/include/ConnectionPool.hpp"
#include "../../ChronoLite/include/Logger.hpp"
#include "../../FixedThreadPool/include/FixedThreadPool.hpp"
#include "../../Inet/include/Buffer.hpp"
#include "../../Inet/include/EventLoop.hpp"
#include "../../Inet/include/InetAddress.hpp"
#include "../../Inet/include/TcpServer.hpp"
#include "../../ScheduledThreadPool/include/ScheduledThreadPool.hpp"

#include <jsoncpp/json/json.h>

#include <any>
#include <atomic>
#include <csignal>
#include <sstream>
#include <memory>
#include <chrono>

using shanchuan::Buffer;
using shanchuan::TcpConnectionPtr;
using hyperticket::makeError;

namespace
{
    // 全局信号标志（信号处理器必须访问全局/静态变量）
    std::atomic<bool> g_sigReceived{false};

    void signalHandler(int)
    {
        g_sigReceived.store(true);
    }

    logsys::LOG_LEVEL parseLogLevel(const std::string &level)
    {
        if (level == "TRACE") return logsys::LOG_LEVEL::TRACE;
        if (level == "DEBUG") return logsys::LOG_LEVEL::DEBUG;
        if (level == "WARN") return logsys::LOG_LEVEL::WARN;
        if (level == "ERROR") return logsys::LOG_LEVEL::ERROR;
        if (level == "FATAL") return logsys::LOG_LEVEL::FATAL;
        return logsys::LOG_LEVEL::INFO;
    }

    void sendJson(const TcpConnectionPtr &conn, const Json::Value &value)
    {
        conn->send(hyperticket::toJsonLine(value));
    }
} // namespace

int main()
{
    std::string configError;
    hyperticket::AppConfig cfg = hyperticket::AppConfig::Load("config.json", &configError);

    logsys::Logger::SetLogLevel(parseLogLevel(cfg.log.level));

    if (!configError.empty())
    {
        LOG_FATAL << "config load failed: " << configError;
        return 1;
    }
    if (!hyperticket::SchemaInitializer::ensureReady(cfg.db))
    {
        LOG_FATAL << "database init failed";
        return 1;
    }

    shanchuan::ConnectionPool *pool = shanchuan::ConnectionPool::GetInstance();
    try
    {
        pool->init(cfg.db.host, cfg.db.user, cfg.db.password, cfg.db.name, cfg.db.port, cfg.db.pool_size, 0);
    }
    catch (const std::exception &e)
    {
        LOG_FATAL << "connection pool init failed: " << e.what();
        return 1;
    }

    shanchuan::FixedThreadPool workerPool(cfg.server.worker_threads);
    shanchuan::ScheduledThreadPool scheduler;

    // 根据配置选择 Session Manager
    std::unique_ptr<hyperticket::ISessionManager> sessionMgr;
    if (cfg.redis.enabled)
    {
        LOG_INFO << "Using Redis Session Manager";
        sessionMgr = std::make_unique<hyperticket::RedisSessionManager>(
            cfg.redis.host,
            cfg.redis.port,
            cfg.redis.session_ttl_minutes * 60 * 1000);
    }
    else
    {
        LOG_INFO << "Using in-memory Session Manager";
        sessionMgr = std::make_unique<hyperticket::SessionManager>();
    }

    // 可选：Redis 库存缓存（预扣减挡在 MySQL 前 + 在售列表缓存）。
    // 初始化失败仅告警并降级为无缓存模式，不影响服务启动。
    std::unique_ptr<hyperticket::RedisConnPool> stockRedisPool;
    std::unique_ptr<hyperticket::RedisStockCache> stockCache;
    if (cfg.redis.enabled)
    {
        try
        {
            stockRedisPool = std::make_unique<hyperticket::RedisConnPool>(
                cfg.redis.host, cfg.redis.port, cfg.redis.pool_size);
            stockCache = std::make_unique<hyperticket::RedisStockCache>(stockRedisPool.get());
            LOG_INFO << "Redis stock cache enabled (pool size " << cfg.redis.pool_size << ")";
        }
        catch (const std::exception &e)
        {
            LOG_ERROR << "Redis stock cache init failed, running without cache: " << e.what();
            stockRedisPool.reset();
            stockCache.reset();
        }
    }

    hyperticket::TicketService service(pool, sessionMgr.get(), stockCache.get());

    // 可选：启动 Metrics Manager
    std::unique_ptr<hyperticket::MetricsManager> metrics;
    if (cfg.metrics.enabled)
    {
        LOG_INFO << "Metrics enabled on port " << cfg.metrics.port;
        metrics = std::make_unique<hyperticket::MetricsManager>(cfg.metrics.port);
    }

    shanchuan::EventLoop loop;
    shanchuan::InetAddress listenAddr(cfg.server.ip, static_cast<uint16_t>(cfg.server.port));
    shanchuan::TcpServer server(&loop, listenAddr, "HyperTicket");
    server.setThreadNum(cfg.server.io_threads);

    std::atomic<int> connCount{0};
    const int maxConn = cfg.server.max_connections;
    const int maxRps = cfg.server.max_requests_per_sec;

    // 连接回调：全局连接数上限 + 为每连接安装令牌桶限流器。
    // 用 CAS 循环保证原子递增+判断，避免 TOCTOU 竞态。
    server.setConnectionCallback([&connCount, maxConn, maxRps](const TcpConnectionPtr &conn) {
        if (conn->connected())
        {
            int old = connCount.load();
            while (old < maxConn)
            {
                if (connCount.compare_exchange_weak(old, old + 1)) break;
            }
            if (old >= maxConn)
            {
                LOG_WARN << "connection limit reached (" << maxConn << "), refusing " << conn->peerAddress().toIp();
                conn->forceClose();
                return;
            }
            conn->setContext(hyperticket::RateLimiter(maxRps));
            LOG_INFO << "connection " << conn->name() << " UP (" << old + 1 << "/" << maxConn << ")";
        }
        else
        {
            --connCount;
            LOG_INFO << "connection " << conn->name() << " DOWN";
        }
    });

    // 消息回调：按 '\n' 拆包 -> 限流 -> 解析 JSON -> 投递业务线程池处理。
    server.setMessageCallback([&workerPool, &service, &metrics](const TcpConnectionPtr &conn, Buffer *buf, shanchuan::Timestamp) {
        while (true)
        {
            const char *eol = buf->findEOL();
            if (!eol) break;
            std::string line(buf->peek(), static_cast<size_t>(eol - buf->peek()));
            buf->retrieveUntil(eol + 1);
            if (line.empty()) continue;

            std::any *ctx = conn->getMutableContext();
            if (ctx && ctx->has_value())
            {
                auto *rl = std::any_cast<hyperticket::RateLimiter>(ctx);
                if (rl && !rl->allow())
                {
                    sendJson(conn, makeError(hyperticket::err::kRateLimited));
                    if (metrics) metrics->recordError("rate_limited");
                    continue;
                }
            }

            Json::Value req;
            Json::CharReaderBuilder builder;
            builder["collectComments"] = false;
            std::string errs;
            std::istringstream iss(line);
            if (!Json::parseFromStream(builder, iss, &req, &errs))
            {
                sendJson(conn, makeError("JSON_PARSE"));
                if (metrics) metrics->recordError("json_parse");
                continue;
            }

            workerPool.add_task([conn, req, &service, &metrics]() {
                auto start = std::chrono::steady_clock::now();
                // 高可用兜底：业务异常绝不允许穿透 worker 线程（未捕获异常会
                // std::terminate 杀死整个进程），统一转为 INTERNAL 错误响应。
                Json::Value resp;
                try
                {
                    resp = service.handle(req);
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR << "handle request failed: " << e.what();
                    resp = makeError("INTERNAL");
                    if (metrics) metrics->recordError("internal");
                }
                catch (...)
                {
                    LOG_ERROR << "handle request failed: unknown exception";
                    resp = makeError("INTERNAL");
                    if (metrics) metrics->recordError("internal");
                }
                auto end = std::chrono::steady_clock::now();

                // 记录 metrics
                if (metrics)
                {
                    double duration = std::chrono::duration<double>(end - start).count();
                    std::string method = "unknown";
                    if (req.isMember("type") && req["type"].isIntegral())
                    {
                        int type = req["type"].asInt();
                        switch (type)
                        {
                        case 1: method = "login"; break;
                        case 2: method = "register"; break;
                        case 3: method = "exit"; break;
                        case 4: method = "view"; break;
                        case 5: method = "order"; break;
                        case 6: method = "view_my"; break;
                        case 7: method = "cancel"; break;
                        case 8: method = "admin_login"; break;
                        case 9: method = "admin_list_tickets"; break;
                        case 10: method = "admin_add_ticket"; break;
                        case 11: method = "admin_delete_ticket"; break;
                        case 12: method = "admin_list_users"; break;
                        case 13: method = "admin_stats"; break;
                        case 14: method = "admin_blacklist"; break;
                        case 15: method = "admin_change_password"; break;
                        case 16: method = "delete_order"; break;
                        case 17: method = "view_seats"; break;
                        case 18: method = "verify_order"; break;
                        case 19: method = "ticket_detail"; break;
                        case 20: method = "pay_order"; break;
                        case 21: method = "favorite"; break;
                        case 22: method = "view_favorites"; break;
                        case 23: method = "hot_tickets"; break;
                        case 24: method = "pay_query"; break;
                        default: method = "unknown"; break;
                        }
                    }

                    const std::string outcome =
                        resp.get(hyperticket::field::kStatus, "").asString() == hyperticket::status::kOk
                            ? "success"
                            : "failed";
                    metrics->recordRequest(method, outcome);
                    metrics->recordRequestDuration(method, duration);

                    if (method == "order")
                    {
                        metrics->recordOrder(outcome);
                    }
                }

                sendJson(conn, resp);
            });
        }
    });

    scheduler.addRunEvery(cfg.schedule.stats_interval_ms, [&service]() { service.logStats(); });
    scheduler.addRunEvery(cfg.schedule.ticket_status_interval_ms, [&service]() { service.refreshTicketStatus(); });
    // 每 30s 回收超时未支付的 PENDING 订单（15 分钟支付窗口）
    scheduler.addRunEvery(30000, [&service]() { service.expirePendingOrders(); });
    // 支付结算：模拟网关异步回调，结算到期的 PROCESSING 流水
    service.configurePayment(cfg.payment.settle_delay_ms, cfg.payment.success_rate_percent);
    scheduler.addRunEvery(cfg.payment.settle_interval_ms, [&service]() { service.settleDuePayments(); });
    scheduler.addRunEvery(60000, [&sessionMgr]() { sessionMgr->purgeExpired(hyperticket::nowMs()); });

    // 定期更新 metrics 资源指标
    if (metrics)
    {
        scheduler.addRunEvery(5000, [&metrics, &pool, &sessionMgr]() {
            metrics->setDbConnectionsActive(pool->GetActiveConn());
            metrics->setDbConnectionsIdle(pool->GetFreeConn());
            // 注意：SessionManager 没有 size() 方法，这里简化处理
            // 如需实现，需要在 ISessionManager 接口添加 size() 方法
        });
    }

    server.start();
    LOG_INFO << "server started at " << cfg.server.ip << ":" << cfg.server.port;

    // 优雅关停：SIGINT/SIGTERM 触发 EventLoop 退出
    // 使用全局原子变量 + 定时器轮询方式
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    // 用定时器轮询信号标志
    scheduler.addRunEvery(1000, [&]() {
        if (g_sigReceived.load())
        {
            LOG_INFO << "shutdown signal received, stopping...";
            loop.quit();
        }
    });

    loop.loop();
    LOG_INFO << "server stopped";
    return 0;
}
