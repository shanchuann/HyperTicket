# 多IO线程TcpConnection析构竞态修复方案

## 问题分析

### 根本原因
在多IO线程模式下（`io_threads > 1`），`TcpConnection` 对象可能在以下场景出现竞态：

1. **跨线程析构**：连接在线程A创建，但在线程B析构
2. **回调执行时对象已销毁**：定时器回调执行时，TcpConnection已被销毁
3. **Channel生命周期问题**：Channel的回调可能在TcpConnection析构后执行

### 当前代码的保护措施
- ✅ 使用 `shared_ptr` 管理 TcpConnection 生命周期
- ✅ 使用 `WeakCallback` 包装延迟回调（`forceCloseWithDelay`）
- ✅ Channel 使用 `tie()` 绑定到 TcpConnection
- ⚠️ 部分回调未使用 `shared_from_this()` 保护

## 修复方案

### 1. 确保所有跨线程回调都使用 shared_ptr

**问题代码：**
```cpp
// startRead/stopRead 使用 this 指针
loop_->runInLoop(std::bind(&TcpConnection::startReadInLoop, this));
loop_->runInLoop(std::bind(&TcpConnection::stopReadInLoop, this));
```

**修复后：**
```cpp
// 使用 shared_from_this() 延长生命周期
loop_->runInLoop(std::bind(&TcpConnection::startReadInLoop, shared_from_this()));
loop_->runInLoop(std::bind(&TcpConnection::stopReadInLoop, shared_from_this()));
```

### 2. 增强状态检查

在所有可能跨线程调用的方法中，增加状态检查：

```cpp
void TcpConnection::startReadInLoop()
{
    loop_->assertInLoopThread();
    // 增加状态检查，避免在已断开的连接上操作
    if (state_ != StateE::kConnected) {
        return;
    }
    if (!reading_ || !channel_->isReading()) {
        channel_->enableReading();
        reading_ = true;
    }
}
```

### 3. 优雅关闭机制

确保连接关闭时，所有pending的回调都能安全执行：

```cpp
void TcpConnection::shutdown()
{
    if (state_ == StateE::kConnected) {
        setState(StateE::kDisconnecting);
        // 使用 shared_from_this() 确保对象在回调执行时仍然存活
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, shared_from_this()));
    }
}
```

### 4. EventLoop 退出时的清理

在 EventLoop 析构前，确保所有连接都已正确关闭：

```cpp
// EventLoop::~EventLoop()
// 1. 停止接受新连接
// 2. 关闭所有现有连接
// 3. 等待所有pending回调执行完毕
// 4. 清理资源
```

## 实施步骤

### Phase 1: 修复 TcpConnection 回调（立即实施）
- [x] 修复 startRead/stopRead 使用 shared_from_this()
- [x] 增强状态检查
- [x] 确保 shutdown 使用 shared_from_this()

### Phase 2: 增强 EventLoop 清理（后续）
- [ ] 实现优雅退出机制
- [ ] 添加连接跟踪
- [ ] 实现超时强制关闭

### Phase 3: 测试验证（后续）
- [ ] 多线程压力测试
- [ ] 连接快速创建/销毁测试
- [ ] 长连接 + 定时器测试

## 风险评估

### 低风险修改（可立即实施）
- ✅ 修复 startRead/stopRead 回调
- ✅ 增强状态检查
- ✅ 文档和注释

### 中风险修改（需要充分测试）
- ⚠️ EventLoop 退出机制改造
- ⚠️ 连接生命周期管理

### 高风险修改（暂不实施）
- ❌ 重构 EventLoop 线程模型
- ❌ 修改 Channel 回调机制

## 当前缓解措施

在完全修复前，建议：
1. ✅ 保持 `io_threads = 1`（已在 config.json 中配置）
2. ✅ 业务逻辑使用独立的 worker 线程池（已实现）
3. ✅ 避免长时间持有连接（通过超时机制）

## 预期效果

修复后：
- ✅ 支持 `io_threads > 1` 的多IO线程模式
- ✅ 消除 `EventLoop::abortNotInLoopThread` 错误
- ✅ 提升并发处理能力（IO线程数 × 性能）
- ✅ 更好的CPU利用率

## 参考

- muduo 网络库的 WeakCallback 机制
- Boost.Asio 的 strand 机制
- libuv 的 handle 生命周期管理
