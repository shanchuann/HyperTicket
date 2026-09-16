FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    libhiredis-dev \
    libjsoncpp-dev \
    libmysqlclient-dev \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY backend ./backend
COPY tests ./tests
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target ser --parallel "$(nproc)"

FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libhiredis0.14 \
    libjsoncpp25 \
    libmysqlclient21 \
    libssl3 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --uid 10001 --shell /usr/sbin/nologin hyperticket

WORKDIR /app
COPY --from=builder /src/bin/ser /app/ser
COPY config.example.json /app/config.json
RUN mkdir -p /app/logs && chown -R hyperticket:hyperticket /app

USER hyperticket
EXPOSE 7000
ENV SERVER_IP=0.0.0.0 \
    SERVER_PORT=7000 \
    REDIS_ENABLED=true

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
  CMD bash -c '</dev/tcp/127.0.0.1/7000' || exit 1

CMD ["/app/ser"]
