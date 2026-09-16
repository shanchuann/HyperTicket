#include "AppConfig.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>

int main()
{
    setenv("DB_HOST", "mysql.internal", 1);
    setenv("DB_POOL_SIZE", "17", 1);
    setenv("SERVER_IP", "0.0.0.0", 1);
    setenv("SERVER_PORT", "7100", 1);
    setenv("REDIS_HOST", "redis.internal", 1);
    setenv("REDIS_PORT", "6380", 1);
    setenv("REDIS_ENABLED", "true", 1);
    setenv("METRICS_ENABLED", "1", 1);

    std::string error;
    const auto config = hyperticket::AppConfig::Load("/tmp/hyperticket-config-does-not-exist.json", &error);
    assert(config.db.host == "mysql.internal");
    assert(config.db.pool_size == 17);
    assert(config.server.ip == "0.0.0.0");
    assert(config.server.port == 7100);
    assert(config.redis.host == "redis.internal");
    assert(config.redis.port == 6380);
    assert(config.redis.enabled);
    assert(config.metrics.enabled);
    assert(!error.empty());

    std::cout << "AppConfig environment override tests passed\n";
    return 0;
}
