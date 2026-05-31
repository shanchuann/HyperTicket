# HyperTicket Frontend

HyperTicket 高性能票务系统的 Web 前端界面

## 技术栈

| 技术 | 版本 | 用途 |
|------|------|------|
| React | ^18.x | UI 框架 |
| TypeScript | ^5.x | 类型安全 |
| Vite | ^5.x | 构建工具 |
| React Router | ^6.x | 客户端路由 |
| Framer Motion | ^11.x | 动画库 |
| Lucide React | ^0.x | 图标库 |

## 项目结构

```
frontend/
├── src/
│   ├── api/              # API 调用层
│   │   ├── client.ts     # WebSocket 客户端（桥接 TCP + JSON）
│   │   ├── auth.ts       # 认证 API
│   │   ├── tickets.ts    # 票务 API
│   │   └── orders.ts     # 订单 API
│   ├── components/       # 通用组件（预留）
│   ├── pages/            # 页面组件
│   │   ├── Landing/      # 营销落地页
│   │   │   ├── Landing.tsx
│   │   │   ├── Landing.css
│   │   │   └── index.ts
│   │   ├── Auth/         # 登录注册
│   │   │   ├── AuthLayout.tsx
│   │   │   ├── AuthLayout.css
│   │   │   ├── Login.tsx
│   │   │   ├── Register.tsx
│   │   │   ├── AuthForms.css
│   │   │   └── index.ts
│   │   ├── Customer/     # 客户端界面
│   │   │   ├── CustomerLayout.tsx
│   │   │   ├── CustomerLayout.css
│   │   │   ├── TicketList.tsx
│   │   │   ├── TicketList.css
│   │   │   ├── OrderList.tsx
│   │   │   ├── OrderList.css
│   │   │   └── index.ts
│   │   └── Admin/        # 管理后台
│   │       ├── AdminLayout.tsx
│   │       ├── AdminLayout.css
│   │       ├── Dashboard.tsx
│   │       ├── Dashboard.css
│   │       ├── TicketManage.tsx
│   │       ├── TicketManage.css
│   │       └── index.ts
│   ├── hooks/            # 自定义 Hooks
│   │   ├── useTheme.ts   # 主题切换
│   │   └── useAuth.ts    # 认证状态
│   ├── store/            # 状态管理（预留）
│   ├── types/            # TypeScript 类型定义
│   │   └── index.ts      # API 类型、用户类型等
│   ├── utils/            # 工具函数（预留）
│   ├── styles/           # 全局样式
│   │   └── tokens.css    # 设计 tokens（颜色、字体、间距等）
│   ├── assets/           # 静态资源（预留）
│   ├── App.tsx           # 根组件
│   └── main.tsx          # 入口文件
├── public/               # 公共资源
├── package.json
├── tsconfig.json
├── vite.config.ts
└── README.md
```

## 设计系统

设计系统定义在 `/DESIGN.md` 和 `src/styles/tokens.css` 中：

### 颜色
- **品牌色**：深青色 (#274C5C) + 青绿色 (#47988F)
- **语义色**：黄色（警告）、橙色（成功）、珊瑚红（错误）
- **主题**：支持日夜间切换

### 字体
- **Display**：ZCOOL XiaoWei（艺术字）用于大标题
- **Body**：PingFang SC / Microsoft YaHei 用于界面
- **Mono**：SF Mono 用于代码和数字

### 风格
- 无边框、扁平、锐角
- 通过背景色和留白建立层级
- 毛玻璃效果仅用于特定元素（如滚动后的导航栏）

## 开发指南

### 安装依赖

```bash
cd frontend
npm install
```

### 启动开发服务器

```bash
npm run dev
```

默认访问 http://localhost:3000

### 构建生产版本

```bash
npm run build
```

构建输出到 `dist/` 目录

### 预览生产构建

```bash
npm run preview
```

## 页面路由

| 路径 | 页面 | 说明 |
|------|------|------|
| `/` | 营销落地页 | 品牌展示、功能介绍 |
| `/auth/login` | 登录页 | 手机号 + 密码登录 |
| `/auth/register` | 注册页 | 新用户注册 |
| `/customer` | 票务浏览 | 客户首页，搜索筛选票务 |
| `/customer/orders` | 我的订单 | 订单列表管理 |
| `/admin` | 管理仪表盘 | 统计数据、最近订单 |
| `/admin/tickets` | 票务管理 | 票务增删改查 |

## WebSocket 桥接层

由于后端使用 TCP + JSON 协议（按 `\n` 分隔），前端通过 WebSocket 桥接层与后端通信：

1. WebSocket 服务器监听 8080 端口（需单独实现）
2. 接收前端 WebSocket 连接
3. 转发消息到后端 TCP 服务（localhost:7000）
4. 将后端响应转发回前端

### 消息格式

```json
// 请求
{"type": 1, "tel": "13800138000", "password": "Password123"}

// 响应
{"success": true, "message": "登录成功", "data": {"token": "...", "username": "..."}}
```

## 环境变量

创建 `.env` 文件：

```env
VITE_WS_URL=ws://localhost:8080
```

## 开发顺序

1. ✅ 营销落地页
2. ✅ 注册登录页面
3. ✅ 客户界面（票务浏览、订单管理）
4. ✅ 管理后台（仪表盘、票务管理）

## 注意事项

- 所有 UI 文案使用中文（项目名称 "HyperTicket" 除外）
- 确保日夜间主题的对比度符合 WCAG 2.1 AA 标准
- 所有交互元素支持键盘导航
- 动画提供 `prefers-reduced-motion` 降级方案
- 图标使用 Lucide React，不使用表情符号
