#!/usr/bin/env python3
"""HyperTicket 并发抢票压测:验证 Redis 预扣减防超卖 + 无票秒拒。

用法: python3 bench_order.py [ticket_id] [concurrency]
每个并发用户独立 TCP 连接(避开单连接限流),注册/登录后同时下单同一张限量票。
"""
import json
import socket
import sys
import threading
import time

HOST, PORT = "127.0.0.1", 7000
TICKET_ID = int(sys.argv[1]) if len(sys.argv) > 1 else 5
CONCURRENCY = int(sys.argv[2]) if len(sys.argv) > 2 else 100


def rpc(sock, obj, f):
    sock.sendall((json.dumps(obj) + "\n").encode())
    line = f.readline()
    if not line:
        raise ConnectionError("server closed")
    return json.loads(line)


results = []
lock = threading.Lock()
barrier = threading.Barrier(CONCURRENCY)


def worker(i):
    tel = f"139{i:08d}"
    outcome = "EXC"
    latency = 0.0
    try:
        sock = socket.create_connection((HOST, PORT), timeout=10)
        f = sock.makefile("r")
        # 注册(已存在则忽略错误)再登录
        rpc(sock, {"type": 2, "usertel": tel, "passward": "Bench123",
                   "username": f"bench{i}"}, f)
        r = rpc(sock, {"type": 1, "usertel": tel, "passward": "Bench123"}, f)
        token = r.get("token", "")
        if not token:
            outcome = "NOLOGIN"
            raise RuntimeError(r.get("reason", "login failed"))

        barrier.wait()  # 全体就绪后同时开抢
        t0 = time.monotonic()
        r = rpc(sock, {"type": 5, "token": token, "index": str(TICKET_ID)}, f)
        latency = (time.monotonic() - t0) * 1000
        outcome = "OK" if r.get("status") == "OK" else r.get("reason", "ERR")
        sock.close()
    except Exception as e:
        if outcome == "EXC":
            outcome = f"EXC:{type(e).__name__}"
    with lock:
        results.append((outcome, latency))


threads = [threading.Thread(target=worker, args=(i,)) for i in range(CONCURRENCY)]
t_start = time.monotonic()
for t in threads:
    t.start()
for t in threads:
    t.join()
elapsed = time.monotonic() - t_start

from collections import Counter
counts = Counter(o for o, _ in results)
ok_lat = sorted(l for o, l in results if o == "OK")
rej_lat = sorted(l for o, l in results if o == "NO_TICKET")

print(f"并发数: {CONCURRENCY}  票: {TICKET_ID}  总耗时: {elapsed:.2f}s")
for k, v in counts.most_common():
    print(f"  {k}: {v}")
if ok_lat:
    print(f"下单成功延迟 p50={ok_lat[len(ok_lat)//2]:.1f}ms max={ok_lat[-1]:.1f}ms")
if rej_lat:
    print(f"无票拒绝延迟 p50={rej_lat[len(rej_lat)//2]:.1f}ms max={rej_lat[-1]:.1f}ms")
