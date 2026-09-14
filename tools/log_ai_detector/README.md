# HyperTicket Log AI Detector

基于 ChronoLite 日志、codegraph 代码索引和 DeepSeek LLM 的 ERROR/FATAL 自动分析工具，附带独立 Web UI。

## 架构

```
ChronoLite logs (logs/*.log)
   │  轮询 tail（inode + offset，支持滚动）
   ▼
daemon.py ── 检测 ERROR/FATAL ──► analyzer.py
   │                                 │  ├─ code_index.py → codegraph / 源码 grep
   │                                 │  ├─ 最近 feat commit diff
   │                                 │  └─ llm_client.py → DeepSeek API
   │                                 ▼
   │                         reporter.py → reports/log-ai-detector/*.md
   │                                 │
   │                         notifier.py → webhook / email
   ▼
server.py（HTTP :7070）
   ├─ /log-ai-detector/          → 独立 Web UI（tools/log-ai-detector-ui/dist/）
   └─ /log-ai-detector/api/*     → 状态 / 报告 / 日志 / 模型切换 / 单次分析
```

## 模块说明

| 文件 | 职责 |
|---|---|
| `daemon.py` | CLI 入口 + 常驻监控循环（去重、debounce） |
| `server.py` | HTTP API + 静态 UI 服务 |
| `chrono_parser.py` | ChronoLite 日志行解析（level/file/function/line） |
| `log_tailer.py` | 轮询 tailer，按 inode+offset 处理日志滚动 |
| `analyzer.py` | 组装上下文 → LLM → 报告 → 通知 |
| `llm_client.py` | OpenAI-compatible HTTP 客户端 |
| `code_index.py` | codegraph 集成（命令模板可配置），降级为源码定位 |
| `reporter.py` | Markdown 报告生成 |
| `notifier.py` | webhook / email 通知抽象 |
| `git_hook.py` | feat commit 检测 + codegraph 增量更新 |
| `config.py` | `.env` 配置加载（LOG_AI_* / DEEPSEEK_*） |

## 快速开始

```bash
# 1. 确认 .env 中已配置（DEEPSEEK_API_KEY 必需）
scripts/log-ai-detector --check-config

# 2. 构建独立 Web UI（首次或 UI 变更后）
cd tools/log-ai-detector-ui && npm install && npm run build && cd ../..

# 3. 启动监控 + Web 服务
scripts/log-ai-detector --serve

# 浏览器访问
#   http://localhost:7070/log-ai-detector/
```

### 其他启动方式

```bash
scripts/log-ai-detector                  # 仅监控（无 HTTP）
scripts/log-ai-detector --serve-only     # 仅 HTTP（不监控）
scripts/log-ai-detector --once --file logs/xxx.log   # 单次分析
```

## 模型

默认 `deepseek-v4-flash`，可通过 Web UI 右上角切换到 `deepseek-v4-pro`（写入 `.log-ai-detector-model` 持久化）。

```env
LOG_AI_LLM_BASE_URL=https://api.deepseek.com
LOG_AI_LLM_API_KEY=<你的 key，或复用 DEEPSEEK_API_KEY>
LOG_AI_LLM_MODEL=deepseek-v4-flash
LOG_AI_AVAILABLE_MODELS=deepseek-v4-flash,deepseek-v4-pro
```

## HTTP API

均挂载于 `/log-ai-detector/api/`：

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/status` | daemon 运行状态、当前模型 |
| GET | `/models` | 可用模型列表 |
| POST | `/model` | `{"model": "..."}` 切换模型 |
| GET | `/reports?limit=100` | 报告列表（新→旧） |
| GET | `/reports/:name` | 单份报告全文 |
| GET | `/logs?n=200` | daemon 运行日志 |
| POST | `/analyze` | `{"file": "..."}` 单次分析 |
| GET | `/config` | 脱敏配置 |

## 通知

检测到错误并生成报告后，可推送通知（收到通知后可在 Web UI 查看报告详情）：

```env
LOG_AI_NOTIFY_CHANNELS=webhook,email
LOG_AI_WEBHOOK_URL=https://your-webhook
LOG_AI_SMTP_HOST=smtp.example.com
LOG_AI_SMTP_FROM=alert@example.com
LOG_AI_SMTP_TO=you@example.com
```

## codegraph 集成

上游 CLI 可能变化，通过命令模板配置（占位符：`{bin}` `{project_root}` `{index_dir}` `{query}` `{diff_file}` `{changed_files}`）：

```env
LOG_AI_CODEGRAPH_BIN=codegraph
LOG_AI_CODEGRAPH_INDEX_CMD={bin} index {project_root} --output {index_dir}
LOG_AI_CODEGRAPH_SEARCH_CMD={bin} search --index {index_dir} {query}
```

feat commit 自动更新索引：

```bash
scripts/update-code-index-on-feat-commit --install-hook
```

安装后，`feat:` / `feat(scope):` commit 触发增量更新；非 feat commit 跳过；失败不阻断 commit。

## 依赖

仅 Python 3.10+ 标准库（见 `tools/requirements.txt`）。Web UI 需 Node.js 构建。

## systemd 部署

```bash
cp deploy/systemd/hyperticket-log-ai-detector.service.example \
   /etc/systemd/system/hyperticket-log-ai-detector.service
# 编辑 ExecStart 加上 --serve，然后：
systemctl enable --now hyperticket-log-ai-detector
```
