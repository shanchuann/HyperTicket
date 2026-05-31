// HyperTicket 服务端入口：仅负责组装（网络 + 线程池 + 调度 + 限流），
// 业务逻辑下沉到 Domain/service/TicketService，协议下沉到 Common/Protocol。
#include "../../Common/include/AppConfig.hpp"
#include "../../Common/include/Protocol.hpp"
#include "../../Common/include/Errors.hpp"
#include "../include/SessionManager.hpp"
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
    hyperticket::SessionManager sessions;
    hyperticket::TicketService service(pool, &sessions);

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
    server.setMessageCallback([&workerPool, &service](const TcpConnectionPtr &conn, Buffer *buf, shanchuan::Timestamp) {
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
                continue;
            }

            workerPool.add_task([conn, req, &service]() {
                sendJson(conn, service.handle(req));
            });
        }
    });

    scheduler.addRunEvery(cfg.schedule.stats_interval_ms, [&service]() { service.logStats(); });
    scheduler.addRunEvery(cfg.schedule.ticket_status_interval_ms, [&service]() { service.refreshTicketStatus(); });
    scheduler.addRunEvery(60000, [&sessions]() { sessions.purgeExpired(hyperticket::nowMs()); });

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
