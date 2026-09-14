# HyperTicket 客户端

## 目录结构

```
clients/
├── user-client/    # Tauri 2 用户客户端（桌面 + Android APK）
│                   # React + TypeScript + Rust TCP
└── admin-client/   # Tauri 2 管理员客户端（桌面）
                    # React + TypeScript + Rust TCP
```

## 用户客户端（user-client）

技术栈：Tauri 2.0 + Rust + React + TypeScript

- Rust 层直连后端 TCP（替代 WebSocket bridge）
- React 层复用 frontend/ 的页面组件
- `tauri android build` 打包 APK

### 环境依赖
```bash
cargo install tauri-cli --version "^2"
rustup target add aarch64-linux-android    # Android 支持
```

## 管理员客户端（admin-client）

技术栈：React + TypeScript + Vite + Tauri 2 + Rust

- Rust `send_request` 命令直连后端 TCP（`127.0.0.1:7000`）
- React 页面与用户端统一工程模式
- 不再依赖 Qt、QML 或 CMake

### 开发
```bash
cd clients/admin-client
npm install
npm run dev       # Web 调试界面：http://localhost:5174
npm run tauri dev # Tauri 桌面客户端
```
