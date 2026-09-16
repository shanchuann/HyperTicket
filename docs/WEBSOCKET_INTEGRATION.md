# HyperTicket WebSocket 集成指南

浏览器无法直接访问 HyperTicket 的 TCP JSON Lines 协议，因此开发环境使用 Node.js 桥接服务在 WebSocket 和 TCP 之间透明转发。Tauri 桌面模式由 Rust 层直连 TCP，不经过该桥接。

## 架构

```text
用户端 http://localhost:5173 ─┐
                              ├─ WebSocket ws://localhost:8080
管理端 http://localhost:5174 ─┘          │
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
| `WS_PORT` | `8080` | WebSocket 监听端口 |
| `TCP_HOST` | `127.0.0.1` | C++ 服务地址 |
| `TCP_PORT` | `7000` | C++ 服务端口 |

客户端通过 `VITE_WS_URL` 覆盖默认桥接地址：

```env
VITE_WS_URL=ws://127.0.0.1:8080
```

桥接层不解释业务协议。它为每个 WebSocket 连接建立对应 TCP 连接，把 JSON 文本补齐换行符后转发，并把后端按行返回的 JSON 发回浏览器。

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

生产环境应在反向代理或网关处启用 HTTPS/WSS，限制 Origin、连接数、消息大小和频率，并配置超时、访问日志与真实客户端 IP 传递。TCP `7000` 不应直接暴露到公网。桥接本身不替代认证、风控或应用层授权。
