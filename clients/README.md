# HyperTicket 客户端

## 目录结构

```
clients/
├── user-client/    # Tauri 2 用户客户端（桌面 + Android APK）
│                   # React + TypeScript + Rust TCP
└── admin-client/   # Qt6 管理员客户端（桌面）
                    # QML + C++ + Material Design 3
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

技术栈：Qt6 + QML + C++

- `QTcpSocket` 直连后端 TCP
- QML UI + Qt Quick Controls 2
- Material Design 3 组件库：https://github.com/sudoevolve/material-components-qml

### 环境依赖
```bash
sudo apt install qt6-base-dev qt6-declarative-dev qt6-tools-dev \
  qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-window libqt6svg6-dev
```
