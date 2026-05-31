#!/bin/bash

# HyperTicket 性能基准测试脚本
# 测试不同 IO 线程配置下的性能表现

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
RESULTS_DIR="$PROJECT_DIR/benchmark_results"
CONFIG_FILE="$PROJECT_DIR/config.json"
SERVER_BIN="$PROJECT_DIR/bin/ser"

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 测试参数
DURATION=30s
CONNECTIONS=100
THREADS=4

# 创建结果目录
mkdir -p "$RESULTS_DIR"

echo "=========================================="
echo "HyperTicket 性能基准测试"
echo "=========================================="
echo ""

# 检查依赖
check_dependencies() {
    echo "检查依赖..."

    if ! command -v wrk &> /dev/null; then
        echo -e "${YELLOW}警告: wrk 未安装，尝试使用 ab${NC}"
        if ! command -v ab &> /dev/null; then
            echo -e "${RED}错误: 需要安装 wrk 或 ab${NC}"
            echo "安装 wrk: sudo apt install wrk"
            echo "安装 ab: sudo apt install apache2-utils"
            exit 1
        fi
        USE_AB=1
    else
        USE_AB=0
    fi

    if ! command -v jq &> /dev/null; then
        echo -e "${YELLOW}警告: jq 未安装，部分功能受限${NC}"
        echo "安装: sudo apt install jq"
    fi

    echo -e "${GREEN}依赖检查完成${NC}"
    echo ""
}

# 停止服务器
stop_server() {
    echo "停止服务器..."
    pkill -9 ser 2>/dev/null || true
    sleep 2
}

# 启动服务器
start_server() {
    local io_threads=$1
    echo "启动服务器（IO 线程数: $io_threads）..."

    # 更新配置文件
    if command -v jq &> /dev/null; then
        jq ".server.io_threads = $io_threads" "$CONFIG_FILE" > "$CONFIG_FILE.tmp"
        mv "$CONFIG_FILE.tmp" "$CONFIG_FILE"
    else
        sed -i "s/\"io_threads\": [0-9]*/\"io_threads\": $io_threads/" "$CONFIG_FILE"
    fi

    # 启动服务器
    cd "$PROJECT_DIR"
    nohup "$SERVER_BIN" > "$RESULTS_DIR/server_${io_threads}threads.log" 2>&1 &
    SERVER_PID=$!

    # 等待服务器启动
    sleep 3

    # 检查服务器是否运行
    if ! ps -p $SERVER_PID > /dev/null; then
        echo -e "${RED}错误: 服务器启动失败${NC}"
        cat "$RESULTS_DIR/server_${io_threads}threads.log"
        exit 1
    fi

    echo -e "${GREEN}服务器启动成功 (PID: $SERVER_PID)${NC}"
}

# 使用 wrk 进行测试
test_with_wrk() {
    local io_threads=$1
    local test_name=$2
    local url=$3
    local body=$4

    echo "测试: $test_name (IO 线程: $io_threads)"

    local output_file="$RESULTS_DIR/${test_name}_${io_threads}threads.txt"

    if [ -n "$body" ]; then
        # POST 请求
        wrk -t$THREADS -c$CONNECTIONS -d$DURATION \
            -s <(cat <<EOF
wrk.method = "POST"
wrk.body   = "$body"
wrk.headers["Content-Type"] = "application/json"
EOF
) "$url" > "$output_file" 2>&1
    else
        # GET 请求
        wrk -t$THREADS -c$CONNECTIONS -d$DURATION "$url" > "$output_file" 2>&1
    fi

    # 解析结果
    local qps=$(grep "Requests/sec:" "$output_file" | awk '{print $2}')
    local latency_avg=$(grep "Latency" "$output_file" | awk '{print $2}')
    local latency_max=$(grep "Latency" "$output_file" | awk '{print $4}')

    echo "  QPS: $qps"
    echo "  平均延迟: $latency_avg"
    echo "  最大延迟: $latency_max"
    echo ""
}

# 使用 ab 进行测试
test_with_ab() {
    local io_threads=$1
    local test_name=$2
    local url=$3
    local body=$4

    echo "测试: $test_name (IO 线程: $io_threads)"

    local output_file="$RESULTS_DIR/${test_name}_${io_threads}threads.txt"
    local requests=$((CONNECTIONS * 100))

    if [ -n "$body" ]; then
        # POST 请求
        echo "$body" > /tmp/post_data.json
        ab -n $requests -c $CONNECTIONS -p /tmp/post_data.json \
           -T "application/json" "$url" > "$output_file" 2>&1
    else
        # GET 请求
        ab -n $requests -c $CONNECTIONS "$url" > "$output_file" 2>&1
    fi

    # 解析结果
    local qps=$(grep "Requests per second:" "$output_file" | awk '{print $4}')
    local latency_mean=$(grep "Time per request:" "$output_file" | head -1 | awk '{print $4}')
    local latency_p99=$(grep "99%" "$output_file" | awk '{print $2}')

    echo "  QPS: $qps"
    echo "  平均延迟: ${latency_mean}ms"
    echo "  P99 延迟: ${latency_p99}ms"
    echo ""
}

# 运行测试套件
run_test_suite() {
    local io_threads=$1

    echo "=========================================="
    echo "测试配置: $io_threads IO 线程"
    echo "=========================================="
    echo ""

    # 启动服务器
    start_server $io_threads

    # 等待服务器稳定
    sleep 2

    # 测试场景
    if [ $USE_AB -eq 1 ]; then
        test_with_ab $io_threads "view_tickets" "http://127.0.0.1:7000/" '{"type":4}'
        test_with_ab $io_threads "login" "http://127.0.0.1:7000/" '{"type":1,"tel":"13800138000","pwd":"password123"}'
    else
        test_with_wrk $io_threads "view_tickets" "http://127.0.0.1:7000/" '{"type":4}'
        test_with_wrk $io_threads "login" "http://127.0.0.1:7000/" '{"type":1,"tel":"13800138000","pwd":"password123"}'
    fi

    # 停止服务器
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
- **测试工具**: $([ $USE_AB -eq 1 ] && echo "Apache Bench (ab)" || echo "wrk")
- **测试时长**: $DURATION
- **并发连接数**: $CONNECTIONS
- **测试线程数**: $THREADS

## 测试场景

1. **查看票务列表** (VIEW)
   - 请求类型: POST
   - 负载: {"type":4}

2. **用户登录** (LOGIN)
   - 请求类型: POST
   - 负载: {"type":1,"tel":"13800138000","pwd":"password123"}

## 测试结果

EOF

    for threads in 1 2 4; do
        echo "### $threads IO 线程配置" >> "$report_file"
        echo "" >> "$report_file"

        for test in view_tickets login; do
            local file="$RESULTS_DIR/${test}_${threads}threads.txt"
            if [ -f "$file" ]; then
                echo "#### $test" >> "$report_file"
                echo '```' >> "$report_file"
                if [ $USE_AB -eq 1 ]; then
                    grep -E "Requests per second:|Time per request:|Transfer rate:|50%|90%|99%" "$file" >> "$report_file"
                else
                    grep -E "Requests/sec:|Latency|Transfer/sec" "$file" >> "$report_file"
                fi
                echo '```' >> "$report_file"
                echo "" >> "$report_file"
            fi
        done
    done

    cat >> "$report_file" <<EOF

## 结论

### 性能对比

| IO 线程数 | 场景 | QPS | 平均延迟 | P99 延迟 |
|----------|------|-----|---------|---------|
EOF

    # 这里可以添加更详细的对比表格

    cat >> "$report_file" <<EOF

### 建议配置

根据测试结果：

- **低负载场景** (< 100 QPS): 1-2 个 IO 线程
- **中等负载场景** (100-1000 QPS): 2-4 个 IO 线程
- **高负载场景** (> 1000 QPS): 4 个 IO 线程

### 注意事项

1. 实际性能受硬件配置影响
2. 数据库性能是主要瓶颈
3. 建议根据实际负载调整配置
4. 监控 CPU 使用率和延迟指标

## 详细日志

所有测试日志保存在: \`$RESULTS_DIR\`
EOF

    echo -e "${GREEN}报告已生成: $report_file${NC}"
    echo ""
}

# 主函数
main() {
    check_dependencies

    # 测试不同 IO 线程配置
    for threads in 1 2 4; do
        run_test_suite $threads
    done

    # 生成报告
    generate_report

    echo "=========================================="
    echo -e "${GREEN}性能测试完成！${NC}"
    echo "=========================================="
    echo ""
    echo "查看报告: cat $RESULTS_DIR/performance_report.md"
    echo "查看详细日志: ls -lh $RESULTS_DIR/"
}

# 运行主函数
main
