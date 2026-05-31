# HyperTicket Server Dockerfile
# 多阶段构建：减小镜像体积，提升安全性

# ============================================================
# Stage 1: 构建阶段
# ============================================================
FROM ubuntu:22.04 AS builder

# 设置非交互式安装，避免tzdata等包的交互提示
ENV DEBIAN_FRONTEND=noninteractive

# 安装构建依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libjsoncpp-dev \
    libmysqlclient-dev \
    libssl-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /build

# 复制源代码
COPY . .

# 构建项目
RUN mkdir -p build && cd build && \
    cmake .. && \
    cmake --build . --target all -j$(nproc) && \
    ctest --output-on-failure

# ============================================================
# Stage 2: 运行阶段（最小化镜像）
# ============================================================
FROM ubuntu:22.04

# 设置非交互式
ENV DEBIAN_FRONTEND=noninteractive

# 只安装运行时依赖（不包含编译工具）
RUN apt-get update && apt-get install -y \
    libjsoncpp25 \
    libmysqlclient21 \
    libssl3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# 创建非root用户运行应用（安全最佳实践）
RUN useradd -m -u 1000 -s /bin/bash hyperticket

# 设置工作目录
WORKDIR /app

# 从构建阶段复制可执行文件
COPY --from=builder /build/bin/ser /app/ser
COPY --from=builder /build/bin/client /app/client
COPY --from=builder /build/bin/admin /app/admin

# 复制配置文件模板
COPY config.example.json /app/config.example.json
COPY .env.example /app/.env.example

# 创建日志目录
RUN mkdir -p /app/logs && chown -R hyperticket:hyperticket /app

# 切换到非root用户
USER hyperticket

# 暴露服务端口
EXPOSE 7000

# 健康检查（每30秒检查一次，超时3秒，失败3次标记为unhealthy）
HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
    CMD test -f /app/ser || exit 1

# 设置环境变量
ENV LOG_LEVEL=INFO

# 启动服务
CMD ["/app/ser"]
