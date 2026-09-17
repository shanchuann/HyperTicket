# HyperTicket 客户端

用户端和管理端使用同一套基础技术：React 18.3、TypeScript 5.6、Vite 6、Tauri 2 和 Lucide React。当前仓库不再维护 Qt/QML 客户端，也没有独立的旧 `frontend/` 应用。

## 目录

| 目录 | 默认开发端口 | 主要能力 |
|---|---:|---|
| `clients/user-client` | 5173 | 目录浏览、详情、选座、订单、模拟支付、电子票、个人中心 |
| `clients/admin-client` | 5174 | 活动、场馆、厅馆、场次、票档和提醒审计管理 |

## 两种运行模式

### 浏览器模式

浏览器通过 `websocket-bridge` 将 WebSocket 消息转发到仅内网可达的 C++ TCP `7000`：

```powershell
cd websocket-bridge
npm install
npm start

cd ..\clients\user-client
npm install
npm run dev

cd ..\admin-client
npm install
npm run dev -- --port 5174
```

默认桥接地址为 `ws://127.0.0.1:8080/ws`。详情见 [WebSocket 集成指南](../docs/WEBSOCKET_INTEGRATION.md)。

### Tauri 桌面模式

Tauri 与浏览器共用 WebSocket 传输层，因此本地开发也需要先启动桥接：

```powershell
cd clients\user-client
npm install
npm run tauri dev
```

管理端使用相同命令，只需切换到 `clients/admin-client`。Windows 需要 Rust、Node.js、Visual Studio C++ Build Tools 和 WebView2。发布构建必须通过仓库变量 `PUBLIC_WS_URL` 注入公开的 `wss://` 地址。

## 用户端页面

| 页面 | 功能 |
|---|---|
| 票务浏览 | 分区推荐、关键词、类型和城市筛选、收藏、开售提醒 |
| 活动详情 | 场馆、场次、票档、须知、地图导航和选座入口 |
| 选座 | 同场多座选择、座位状态、订单金额摘要 |
| 支付 | 模拟二维码、应付金额、支付状态轮询和出票动效 |
| 我的订单 | 待支付、已确认、已取消、已过期及订单详情 |
| 我的电子票 | 票面预览、座位和核销信息 |
| 个人中心 | 资料、头像、联系方式状态、收藏、历史、观演人和账号安全 |

## 管理端页面

- 活动目录：活动、场馆/影院、厅馆、场次和票档标签页。
- 场次编辑：校验 `售票开始 < 售票截止 < 演出开始`，并生成兼容票品与独立座位库存。
- 场次复制：复用已有配置创建新场次。
- 提醒审计：查看订阅、发送结果和失败信息。
- 原有运营模块：用户、订单、统计和安全相关管理。

## 构建检查

```powershell
cd clients\user-client
npm run build

cd ..\admin-client
npm run build
```

提交前还应检查桌面和移动视口下的目录、详情、选座、支付、订单、电子票与个人中心流程。
