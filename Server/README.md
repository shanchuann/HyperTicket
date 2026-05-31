# Server ser

HyperTicket 

## 

- **** [Inet](../Inet/README.md) epoll Reactorone loop per thread** libevent**
- ****`io_threads` IO + [FixedThreadPool](../FixedThreadPool/README.md) 
- ****MySQL 8.0 [SqlConnPool](../SqlConnPool/README.md) 
- ****[ChronoLite](../ChronoLite/README.md) 
- ****[ScheduledThreadPool](../ScheduledThreadPool/README.md)
- ****jsoncpp/ JSON `\n` 
- C++17 `hyperticket` `shanchuan`

## 

### 1. Redis Session Manager

**** Session Redis

****
```json
{
"redis": {
"host": "127.0.0.1",
"port": 6379,
"enabled": true
}
}
```

****
- `include/RedisSessionManager.hpp`
- `src/RedisSessionManager.cpp`
- hiredis 

****[docs/REDIS_SESSION_GUIDE.md](../docs/REDIS_SESSION_GUIDE.md)

### 2. Metrics Manager

**** Prometheus 

****
```json
{
"metrics": {
"port": 8080,
"enabled": true
}
}
```

****
- `hyperticket_requests_total` - 
- `hyperticket_orders_total` - 
- `hyperticket_sessions_active` - 
- `hyperticket_db_connections_active/idle` - 
- `hyperticket_errors_total` - 

****`curl http://localhost:8080/metrics`

****[docs/PROMETHEUS_METRICS_GUIDE.md](../docs/PROMETHEUS_METRICS_GUIDE.md)

## 

1. IO `MessageCallback` `\n` JSON
2. **** `RATE_LIMITED`
3. JSON `JSON_PARSE`
4. `TicketService::handleRequest` `type` 

## `OP_TYPE` `type`

| type | | | token |
|------|------|------|:--------:|
| 1 | LOGIN | token | |
| 2 | REGISTER | | |
| 3 | EXIT | | |
| 4 | VIEW | status=1 | |
| 5 | ORDER | | |
| 6 | VIEW_MY | | |
| 7 | CANCEL | | |

JSON `type``usertel``passward``username``token``index`
`{"status":"OK", ...}` `{"status":"ERR","reason":"..."}`

## 

** token **`SessionManager`
- 32 token64 hex `/dev/urandom` `random_device`
- token → `{tel, userId, expireMs}` worker 
- TTL 30 `resolve()` **** 60 token
- ORDER/VIEW_MY/CANCEL token ** tel**//

**SQL **`MysqlStmt`
- `mysql_stmt_*` `bindString`/`bindInt`
- 

****
- `max_connections` 1000 `forceClose`
- `RateLimiter` = `max_requests_per_sec` 20/ `TcpConnection` context

****
- ORDER/CANCEL `SELECT ... FOR UPDATE` / `reservation_audit` 

## 

`config.json` `config.example.json` `.env` [Common/AppConfig](../Common/README.md)

- `server.ip` / `server.port` `0.0.0.0:7000`
- `server.io_threads` / `server.worker_threads`
- `server.max_connections` / `server.max_requests_per_sec`
- `db.*``db.pool_size``log.*``schedule.*`
- `redis.*`Session 
- `metrics.*`

`db/init.sql`

## 

### SessionManager / RedisSessionManager
- ****`include/SessionManager.hpp`
- **Redis **`include/RedisSessionManager.hpp`
- token 
- `config.json` `redis.enabled` 

### MetricsManager
- `include/MetricsManager.hpp``src/MetricsManager.cpp`
- Prometheus 
- `config.json` `metrics.enabled: true`

### RateLimiter
- `include/RateLimiter.hpp`
- 
- `server.max_requests_per_sec`

### HealthCheck
- `include/HealthCheck.hpp`
- 
- `curl http://localhost:7000/health`

## 

```bash
# 
cmake -S . -B build && cmake --build build -j
./bin/ser # config.json 
```

## 

- `include/ser.hpp``OP_TYPE` 
- `include/SessionManager.hpp` token 
- `include/RedisSessionManager.hpp`Redis token 
- `include/MetricsManager.hpp`Prometheus Metrics 
- `include/RateLimiter.hpp`
- `include/HealthCheck.hpp`
- `include/MysqlStmt.hpp` RAII Domain/include
- `src/ser.cpp``TicketService` + `main` 
- `src/RedisSessionManager.cpp`Redis Session 
- `src/MetricsManager.cpp`Metrics 

## 

### 
- Redis Session`redis.enabled: true`
- Nginx / HAProxy / Kubernetes Service
- MySQL Redis

### 
- Metrics`metrics.enabled: true`
- Prometheus 
- Grafana 
- 

### 
- Kubernetes liveness/readiness probe
- 
- 

## 

### 
- 10-100x 
- 5% → 0.1%
- IO Phase 1
- 

### 
- **QPS**> 10,0008 worker threads
- ****P99 < 100ms
- ****1000+
- ****20 

## 

### 

1. ****
- MySQL 
- config.json 
- `tail -f logs/hyperticket.log`

2. ****
- `lsof -i :7000`
- config.json 

3. ****
- metrics`curl http://localhost:8080/metrics`
- 
- worker_threads db.pool_size

4. ****
- valgrind`valgrind --leak-check=full ./bin/ser`
- shared_ptr 

## 

- [../README.md](../README.md) - 
- [../QUICKSTART.md](../QUICKSTART.md) - 
- [../docs/REDIS_SESSION_GUIDE.md](../docs/REDIS_SESSION_GUIDE.md) - Redis Session 
- [../docs/PROMETHEUS_METRICS_GUIDE.md](../docs/PROMETHEUS_METRICS_GUIDE.md) - Metrics 
- [../docs/MULTITHREAD_FIX.md](../docs/MULTITHREAD_FIX.md) - 
