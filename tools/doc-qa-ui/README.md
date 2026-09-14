# Doc QA UI

HyperTicket 文档问答机器人的独立 Web 前端（对话式界面）。

- Vite + React + TypeScript + lucide-react，零额外运行时依赖（Markdown 渲染为内置实现，无 dangerouslySetInnerHTML）
- 构建产物挂载于 `/doc-qa/` 路径，由 `tools/doc_qa.py --serve` 托管（端口 7171）
- 设计 token 与 HyperTicket 主设计系统对齐（OKLCH、扁平无圆角、日夜双主题）

## 界面

- 左侧：文档索引树（按目录分组，可折叠），呈现分级索引的"第一层"
- 中间：对话线程，每个回答顶部标注本轮实际加载的文档来源（渐进式披露的透明化）
- 空状态：预设问题引导（架构、请求链路、防超卖、双缓冲日志）
- 支持多轮对话（会话隔离）、清除历史、日夜主题

## 开发

```bash
npm install
npm run dev     # http://localhost:5175/doc-qa/（/doc-qa/api 代理至 :7171）
```

需同时运行后端：

```bash
python3 tools/doc_qa.py --serve
```

## 构建与访问

```bash
npm run build   # 输出 dist/，由 doc_qa.py 托管
```

访问：`http://127.0.0.1:7171/doc-qa/`
