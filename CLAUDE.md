# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HyperTicket is a high-performance ticket reservation system written in C++17. It consists of three executables:
- **ser**: Server (handles concurrent requests, database transactions, session management)
- **client**: Interactive CLI client
- **admin**: Database administration tool

The system uses custom-built infrastructure components rather than external frameworks (no libevent, no Boost.Asio).

## Build Commands

```bash
# Full build from scratch
mkdir -p build && cd build
cmake ..
cmake --build . --target all -j$(nproc)

# Executables output to: bin/ser, bin/client, bin/admin

# Run tests
cd build
ctest --output-on-failure

# Run individual test
./test_buffer
./test_timestamp
./test_session
./test_protocol

# Clean rebuild
rm -rf build bin && mkdir build && cd build && cmake .. && cmake --build . -j$(nproc)
```

## Running the System

```bash
# Prerequisites: MySQL running, database initialized
mysql -u root -p < db/init.sql

# Copy and configure
cp config.example.json config.json
# Edit config.json with your database credentials

# Start server (foreground)
./bin/ser

# Or background with logging
nohup ./bin/ser > logs/ser.log 2>&1 &

# Run client (in another terminal)
./bin/client

# Run admin tool
./bin/admin
```

## Architecture

### Layered Structure

```
Client/Admin (presentation)
    ↓
Server (ser.cpp) - request routing, session management
    ↓
Domain (service layer) - business logic, transaction orchestration
    ↓ TicketService, Repository pattern
SqlConnPool - connection pooling
    ↓
MySQL - users, tickets, reservations, reservation_audit
```

### Custom Infrastructure Components

All located in repository root as separate modules:

- **Inet/**: Reactor network library (epoll-based, master-slave Reactor pattern, one loop per thread)
- **ChronoLite/**: Async logging (double-buffering + background thread)
- **FixedThreadPool/**: Business worker thread pool
- **ScheduledThreadPool/**: Periodic tasks (session cleanup, ticket inspection, stats)
- **SqlConnPool/**: MySQL connection pool with health checks
- **Common/**: Unified config loading (AppConfig reads config.json + .env + env vars)

### Request Flow

1. **IO Thread** (Inet TcpServer): epoll event loop → parse JSON lines (delimited by `\n`) → rate limiting
2. **Worker Thread** (FixedThreadPool): `TicketService::handleRequest()` dispatches by `type` field
3. **Database**: Connection borrowed from SqlConnPool, uses prepared statements (`MysqlStmt`)
4. **Response**: JSON written back through TcpConnection

### Key Design Patterns

- **Domain-Driven Design**: Domain/ contains service layer (TicketService) and repository interfaces (header-only)
- **RAII Wrappers**: `MysqlStmt` wraps `mysql_stmt_*` for prepared statements
- **Session Management**: `SessionManager` maintains in-memory token → user mapping with sliding expiration (30 min TTL)
- **Transaction Safety**: ORDER/CANCEL use `SELECT ... FOR UPDATE` within transactions to prevent overselling

## Important Constraints

### Multi-IO Thread Issue

**Known limitation**: `config.json` field `io_threads` must be set to `1`. The Inet network library has a connection destruction race condition when using multiple IO threads (`EventLoop::abortNotInLoopThread` assertion failure). This occurs when connections close immediately after a long transaction.

- Setting `io_threads: 1` is safe and fully functional
- Business concurrency is still achieved via `worker_threads` (default 4)
- **Phase 1 fixes completed**: `TcpConnection::startRead/stopRead` now use `shared_from_this()`
- Fixing the multi-IO thread race is future work in the Inet layer (Phase 2 & 3)

### Configuration Priority

Configuration is loaded in this order (later overrides earlier):
1. `config.json` (required, must exist)
2. `.env` file (optional, keys: `DB_HOST`, `DB_PORT`, `DB_USER`, `DB_PASSWORD`, `DB_NAME`)
3. Process environment variables

Both `config.json` and `.env` are gitignored. Use `config.example.json` and `.env.example` as templates.

### Optional Features

**Redis Session Manager** (disabled by default):
- Set `redis.enabled: true` in config.json to enable
- Requires hiredis library installation for production use
- Current implementation uses file-based placeholder for development
- See `docs/REDIS_SESSION_GUIDE.md` for full integration

**Metrics Manager** (disabled by default):
- Set `metrics.enabled: true` in config.json to enable
- Exposes Prometheus metrics on port 8080 (configurable)
- Lightweight HTTP server, no external dependencies required
- See `docs/PROMETHEUS_METRICS_GUIDE.md` for details

### Database Schema

- Schema is **not** auto-created by the application
- Must manually run `db/init.sql` before first startup
- Tables: `users`, `tickets`, `reservations`, `admins`, `reservation_audit`
- All queries use prepared statements (`mysql_stmt_*`) to prevent SQL injection

## Code Conventions

### Namespaces

- Business code: `namespace hyperticket`
- Infrastructure libraries: `namespace shanchuan`

### Header Organization

- **Domain/include/repository/**: Repository interfaces (header-only)
- **Domain/include/service/**: Service layer (TicketService implementation in src/)
- **Domain/include/MysqlStmt.hpp**: Prepared statement RAII wrapper
- **Server/include/SessionManager.hpp**: Token-based session management
- **Server/include/RateLimiter.hpp**: Token bucket rate limiter per connection

### Security Patterns

When adding new endpoints that require authentication:
1. Check for `token` field in request JSON
2. Call `sessionMgr.resolve(token)` to get user identity
3. **Never trust client-provided `tel` or `user_id`** - always use token-resolved identity
4. Use `MysqlStmt` for all database queries with external input
5. Wrap ORDER/CANCEL operations in transactions with `SELECT ... FOR UPDATE`

## Testing

Tests are in `tests/` and registered with CTest:
- `test_buffer.cpp`: Inet Buffer class
- `test_timestamp.cpp`: ChronoLite Timestamp
- `test_session.cpp`: SessionManager token lifecycle
- `test_protocol.cpp`: JSON protocol parsing

Tests have zero external dependencies (no database, no network).

## Docker Deployment

```bash
# Full stack (app + MySQL + Redis)
docker-compose up -d

# With monitoring (Prometheus + Grafana)
docker-compose --profile monitoring up -d

# Health check
curl http://localhost:7000/health

# View logs
docker-compose logs -f hyperticket
```

## Documentation

- `README.md`: Full project documentation
- `QUICKSTART.md`: Deployment guide with Docker and health checks
- `docs/MULTITHREAD_FIX.md`: Multi-IO thread race condition details
- `docs/REDIS_SESSION_GUIDE.md`: Redis session persistence design (Phase 2)
- `docs/PROMETHEUS_METRICS_GUIDE.md`: Metrics integration design (Phase 2)
- Each module has its own `README.md` (e.g., `Inet/README.md`, `Server/README.md`)

## Common Development Tasks

### Adding a New Request Type

1. Add enum to `Server/include/ser.hpp` `OP_TYPE`
2. Implement handler in `Domain/src/service/TicketService.cpp`
3. Update protocol documentation in `README.md`
4. Add test case in `tests/test_protocol.cpp`

### Modifying Database Schema

1. Update `db/init.sql` (single source of truth)
2. Update corresponding Repository interface in `Domain/include/repository/`
3. Update `MysqlStmt` bindings in service implementation
4. Document migration steps in commit message

### Adding a New Module

Follow the existing pattern:
```
ModuleName/
  include/  # Public headers
  src/      # Implementation
  README.md # Module documentation
```

Update `CMakeLists.txt` to add source files and include directories.
