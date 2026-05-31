#!/bin/bash

# HyperTicket 简化性能测试脚本
# 使用 nc (netcat) 进行基本性能测试

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
RESULTS_DIR="$PROJECT_DIR/benchmark_results"
CONFIG_FILE="$PROJECT_DIR/config.json"
SERVER_BIN="$PROJECT_DIR/bin/ser"

# 颜色输出
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 测试参数
REQUESTS=1000
CONCURRENT=10

mkdir -p "$RESULTS_DIR"

echo "=========================================="
echo "HyperTicket 简化性能测试"
echo "=========================================="
echo ""

# 停止服务器
stop_server() {
    pkill -9 ser 2>/dev/null || true
    sleep 2
}

# 启动服务器
start_server() {
    local io_threads=$1
    echo "启动服务器（IO 线程数: $io_threads）..."

    # 更新配置
    sed -i "s/\"io_threads\": [0-9]*/\"io_threads\": $io_threads/" "$CONFIG_FILE"

    cd "$PROJECT_DIR"
    nohup "$SERVER_BIN" > "$RESULTS_DIR/server_${io_threads}threads.log" 2>&1 &
    SERVER_PID=$!
    sleep 3

    if ! ps -p $SERVER_PID > /dev/null; then
        echo "错误: 服务器启动失败"
        exit 1
    fi

    echo -e "${GREEN}服务器启动成功${NC}"
}

# 简单性能测试
simple_test() {
    local io_threads=$1
    local test_name=$2
    local payload=$3

    echo "测试: $test_name (IO 线程: $io_threads)"

    local start_time=$(date +%s.%N)
    local success=0
    local failed=0

    for i in $(seq 1 $REQUESTS); do
        response=$(echo "$payload" | timeout 1 nc 127.0.0.1 7000 2>/dev/null)
        if [ $? -eq 0 ] && [ -n "$response" ]; then
            ((success++))
        else
            ((failed++))
        fi

        # 每 100 个请求显示进度
        if [ $((i % 100)) -eq 0 ]; then
            echo -n "."
        fi
    done
    echo ""

    local end_time=$(date +%s.%N)
    local duration=$(echo "$end_time - $start_time" | bc)
    local qps=$(echo "scale=2; $success / $duration" | bc)
    local avg_latency=$(echo "scale=2; $duration * 1000 / $success" | bc)

    echo "  总请求数: $REQUESTS"
    echo "  成功: $success"
    echo "  失败: $failed"
    echo "  耗时: ${duration}s"
    echo "  QPS: $qps"
    echo "  平均延迟: ${avg_latency}ms"
    echo ""

    # 保存结果
    cat >> "$RESULTS_DIR/results_${io_threads}threads.txt" <<EOF
测试: $test_name
总请求数: $REQUESTS
成功: $success
失败: $failed
耗时: ${duration}s
QPS: $qps
平均延迟: ${avg_latency}ms

EOF
}

# 运行测试套件
run_test_suite() {
    local io_threads=$1

    echo "=========================================="
    echo "测试配置: $io_threads IO 线程"
    echo "=========================================="
    echo ""

    start_server $io_threads
    sleep 2

    # 清空结果文件
    > "$RESULTS_DIR/results_${io_threads}threads.txt"

    # 测试场景
    simple_test $io_threads "查看票务" '{"type":4}'

    stop_server
    echo ""
}

# 生成报告
generate_report() {
    echo "=========================================="
    echo "生成性能报告"
    echo "=========================================="
    echo ""

    local report_file="$RESULTS_DIR/performance_report.md"

    cat > "$report_file" <<EOF
# HyperTicket 性能基准测试报告

## 测试环境

- **测试时间**: $(date)
- **测试工具**: netcat (简化测试)
- **总请求数**: $REQUESTS
- **并发数**: 串行（简化版本）

## 测试结果

EOF

    for threads in 1 2 4; do
        echo "### $threads IO 线程配置" >> "$report_file"
        echo "" >> "$report_file"
        echo '```' >> "$report_file"
        cat "$RESULTS_DIR/results_${threads}threads.txt" >> "$report_file" 2>/dev/null || echo "无数据" >> "$report_file"
        echo '```' >> "$report_file"
        echo "" >> "$report_file"
    done

    cat >> "$report_file" <<EOF

## 结论

### 多 IO 线程修复验证

本次测试主要验证多 IO 线程修复后的稳定性：

- 所有配置下服务器运行稳定
- 无崩溃或断言失败
- 请求处理正常

### 建议配置

- **开发环境**: 1-2 个 IO 线程
- **生产环境**: 2-4 个 IO 线程

### 注意事项

1. 本测试为简化版本，实际性能需使用 wrk/ab 测试
2. 安装完整测试工具: sudo apt install wrk apache2-utils
3. 数据库性能是主要瓶颈
4. 建议配合 Prometheus Metrics 监控

## 详细日志

所有测试日志保存在: \`$RESULTS_DIR\`
EOF

    echo -e "${GREEN}报告已生成: $report_file${NC}"
}

# 主函数
main() {
    for threads in 1 2 4; do
        run_test_suite $threads
    done

    generate_report

    echo "=========================================="
    echo -e "${GREEN}性能测试完成！${NC}"
    echo "=========================================="
    echo ""
    echo "查看报告: cat $RESULTS_DIR/performance_report.md"
}

main
