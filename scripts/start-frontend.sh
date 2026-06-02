#!/bin/bash

# HyperTicket 前端启动脚本
# 1. 启动 WebSocket 桥接服务器
# 2. 启动前端开发服务器

# 脚本所在 scripts/ 目录的上一级即项目根目录
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

echo "HyperTicket 前端启动"

# 检查后端是否运行
echo ""
echo "检查后端服务..."
if ! nc -z localhost 7000 2>/dev/null; then
    echo "警告: 后端服务未在 localhost:7000 运行"
    echo "请先启动后端: ./bin/ser"
    echo ""
    read -p "是否继续启动前端? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# 启动 WebSocket 桥接服务器
echo ""
echo "启动 WebSocket 桥接服务器..."
cd "$ROOT_DIR/websocket-bridge"
if ! pgrep -f "node server.js" > /dev/null; then
    npm start &
    BRIDGE_PID=$!
    echo "桥接服务器已启动 (PID: $BRIDGE_PID)"
    sleep 2
else
    echo "桥接服务器已在运行"
fi

# 启动前端开发服务器
echo ""
echo "启动前端开发服务器..."
cd "$ROOT_DIR/frontend"
npm run dev

# 清理
echo ""
echo "正在关闭服务..."
if [ -n "$BRIDGE_PID" ]; then
    kill $BRIDGE_PID 2>/dev/null
fi

echo "已关闭"
