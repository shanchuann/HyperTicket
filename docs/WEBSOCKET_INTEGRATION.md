# HyperTicket WebSocket 集成指南

本文档描述如何将前端 Web 界面与后端 TCP 服务通过 WebSocket 桥接连接。

## 架构概览

```
┌─────────────┐      WebSocket       ┌──────────────────┐      TCP       ┌─────────────┐
│   前端 React  │  <================>  │  WebSocket 桥接   │  <==========>  │  HyperTicket │
│   (端口 3000) │   ws://localhost:8080 │   (端口 8080)     │                │  (端口 7000)  │
└─────────────┘                      └──────────────────┘                └─────────────┘
```

## 启动步骤

### 1. 启动后端服务

```bash
cd /home/shanchuan/HyperTicket
./bin/ser
```

确保后端在 localhost:7000 监听。

### 2. 启动 WebSocket 桥接服务器

```bash
cd /home/shanchuan/HyperTicket/websocket-bridge
npm install  # 首次运行
npm start
```

桥接服务器将在 ws://localhost:8080 监听。

### 3. 启动前端开发服务器

```bash
cd /home/shanchuan/HyperTicket/frontend
npm install  # 首次运行
npm run dev
```

前端将在 http://localhost:3000 启动。

### 或者使用一键启动脚本

```bash
cd /home/shanchuan/HyperTicket
./scripts/start-frontend.sh
```

## 环境变量

前端通过环境变量配置 WebSocket 地址：

```env
# frontend/.env
VITE_WS_URL=ws://localhost:8080
```

生产环境可修改为实际的桥接服务器地址。

## API 调用示例

### 登录

```typescript
import { authApi } from './api/auth';

// 登录
const response = await authApi.login('13800138000', 'password123');
console.log(response.data.token);  // 获取 token
```

### 获取票务列表

```typescript
import { ticketApi } from './api/tickets';

const response = await ticketApi.getTickets();
console.log(response.data.tickets);
```

### 创建订单

```typescript
import { orderApi } from './api/orders';

const response = await orderApi.createOrder(token, ticketId, quantity);
```

## 协议格式

### 请求格式

```json
{
  "type": 1,
  "tel": "13800138000",
  "password": "password123"
}
```

### 响应格式

```json
{
  "success": true,
  "message": "登录成功",
  "data": {
    "token": "a1b2c3d4e5f6...",
    "username": "张三",
    "tel": "13800138000"
  }
}
```

## 操作类型 (OP_TYPE)

| type | 名称 | 说明 | 需要 token |
|------|------|------|:----------:|
| 1 | LOGIN | 用户登录 | |
| 2 | REGISTER | 用户注册 | |
| 3 | EXIT | 退出登录 | |
| 4 | VIEW | 查看在售票务 | |
| 5 | ORDER | 下单预订 | 需要 |
| 6 | VIEW_MY | 查看本人订单 | 需要 |
| 7 | CANCEL | 取消预订 | 需要 |

## 故障排查

### 连接失败

1. 检查后端是否在运行：`lsof -i :7000`
2. 检查桥接服务器是否在运行：`lsof -i :8080`
3. 检查浏览器控制台的网络请求

### 消息发送失败

1. 确认 WebSocket 连接状态：`wsClient.isConnected()`
2. 检查请求格式是否符合协议
3. 查看浏览器控制台和桥接服务器日志

### 跨域问题

前端和后端运行在不同端口，WebSocket 桥接解决了跨域问题。确保桥接服务器和前端在同一域名下，或配置 CORS。

## 生产部署

1. 部署 WebSocket 桥接服务器到独立服务器或使用负载均衡
2. 配置 Nginx 反向代理 WebSocket
3. 启用 SSL/TLS (wss://)
4. 配置防火墙规则

### Nginx 配置示例

```nginx
server {
    listen 443 ssl;
    server_name your-domain.com;

    location /ws {
        proxy_pass http://localhost:8080;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }
}
```
