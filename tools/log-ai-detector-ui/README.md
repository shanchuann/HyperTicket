# Log AI Detector UI

日志 AI 检测的独立 Web 前端（与主项目 `frontend/` 完全解耦）。

- Vite + React + TypeScript + lucide-react
- 构建产物挂载于 `/log-ai-detector/` 路径，由 Python 服务（`tools/log_ai_detector/server.py`，端口 7070）直接提供
- 设计 token 与 HyperTicket 主设计系统对齐（OKLCH、无边框扁平风格、日夜双主题）

## 功能

- 状态栏：daemon 运行状态、当前模型、时间戳
- 模型切换：deepseek-v4-flash / deepseek-v4-pro（默认 flash）
- 报告：列表 + 搜索 + 全文查看
- 运行日志：daemon 最近 200 行
- 单次分析：指定日志文件立即扫描 ERROR/FATAL
- 配置查看：脱敏后的运行配置

## 开发

```bash
npm install
npm run dev     # http://localhost:5174/log-ai-detector/
                # /log-ai-detector/api 代理至 127.0.0.1:7070
```

需要同时运行 API 服务：

```bash
scripts/log-ai-detector --serve-only
```

## 构建与访问

```bash
npm run build   # 输出 dist/，由 Python 服务托管
```

访问入口：

| 场景 | URL |
|---|---|
| Python 服务直接访问 | `http://localhost:7070/log-ai-detector/` |
| 经主前端 dev 代理 | `http://localhost:3000/log-ai-detector/` |
