# HyperTicket WebSocket 集成指南

浏览器和 Tauri 桌面端统一使用 WebSocket。Node.js 桥接服务负责静态资源和 WebSocket 到 TCP JSON Lines 的转发，C++ TCP 端口不直接暴露给终端用户。

## 架构

```text
用户端 / 管理端 / Tauri ──────┐
                              ├─ WebSocket ws://localhost:8080/ws
                              ┘          │
                                         ▼
                              websocket-bridge (Node.js)
                                         │ TCP 127.0.0.1:7000
                                         ▼
                              HyperTicket C++ Server
```

## 启动

先在 WSL2 仓库根目录启动 C++ 服务：

```bash
./bin/ser
```

再从 Windows 或 WSL2 启动桥接：

```bash
cd websocket-bridge
npm install
npm start
```

启动客户端：

```bash
cd clients/user-client
npm install
npm run dev
```

```bash
cd clients/admin-client
npm install
npm run dev -- --port 5174
```

## 桥接配置

| 环境变量 | 默认值 | 说明 |
|---|---:|---|
| `WEB_PORT` | `8080` | HTTP 与 WebSocket 监听端口（兼容旧 `WS_PORT`） |
| `TCP_HOST` | `127.0.0.1` | C++ 服务地址 |
| `TCP_PORT` | `7000` | C++ 服务端口 |
| `HYPERTICKET_GATEWAY_TOKEN` | 空 | 与 C++ 服务共享的网关令牌；生产环境必须设置 |
| `WS_ALLOWED_ORIGINS` | 空 | 允许的 Origin，逗号分隔；生产环境必须显式设置 |
| `TRUSTED_PROXY_ADDRESSES` | 空 | 可提供 `X-Forwarded-For` 的代理 IP/CIDR |
| `WS_MAX_CONNECTIONS` | `1000` | Bridge 总连接上限 |
| `WS_MAX_CONNECTIONS_PER_IP` | `20` | 单客户端 IP 连接上限 |
| `WS_MAX_MESSAGE_BYTES` | `65536` | 单条 WebSocket 消息上限 |
| `WS_MAX_PENDING_BYTES` | `262144` | 单连接待发送缓冲上限 |

客户端通过 `VITE_WS_URL` 覆盖默认桥接地址：

```env
VITE_WS_URL=ws://127.0.0.1:8080/ws
```

桥接层不解释业务协议。它为每个 WebSocket 连接建立对应 TCP 连接，注入受共享令牌保护的客户端 IP，再把后端按行返回的 JSON 发回客户端。默认不信任 `X-Forwarded-For`。

## 协议示例

请求：

```json
{"type":4,"keyword":"星环","city":"上海","category":"esports"}
```

成功响应使用 `status: "OK"`，失败响应使用 `status: "ERR"` 和 `reason`。认证请求在载荷中携带后端签发的 `token`。完整操作类型以 [Protocol.hpp](../backend/Common/include/Protocol.hpp) 为准，不在桥接层复制维护。

## 故障排查

1. 使用 `ss -ltnp | grep 7000` 确认 C++ 服务监听。
2. 查看桥接终端是否显示已监听 `8080`，以及 TCP 连接错误。
3. 浏览器开发者工具中检查 WebSocket 握手与消息帧。
4. WSL2 无法访问 Windows MySQL 时，先检查数据库监听地址、防火墙和 `.env` 中的主机地址。
5. 公开目录允许过期的可选 Session 回退为游客；收藏、订单和个人中心仍要求有效 token。

## 生产边界

生产环境必须使用 HTTPS/WSS，并同时配置 Bridge 与后端相同的 `HYPERTICKET_GATEWAY_TOKEN`。`WS_ALLOWED_ORIGINS` 至少包含 Web 站点；桌面包按平台需要加入 `http://tauri.localhost` 和 `tauri://localhost`。只有受信反向代理地址应写入 `TRUSTED_PROXY_ADDRESSES`。TCP `7000` 只在 Compose 内网暴露，桥接本身不替代认证、风控或应用层授权。
