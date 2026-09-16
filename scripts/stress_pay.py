#!/usr/bin/env python3
"""HyperTicket 支付模块并发压测。

直连服务端 TCP（换行分隔的 JSON 协议），用多线程模拟大量用户对同一场次
"下单 → 发起支付 → 轮询结算" 的完整链路，验证：

  1. 不超卖：SUCCEEDED（已支付确认）订单数 <= 该场次总库存；
  2. 支付幂等：同一订单并发多次发起支付，只产生一笔支付流水（金额一致，
     不会重复扣款）；
  3. 库存守恒：结束后 SUCCEEDED 订单数 + 剩余库存 == 初始库存
     （失败/退款/超时订单的库存都应被回补）。

用法：
  python3 scripts/stress_pay.py --host 127.0.0.1 --port 7000 \
      --ticket 1 --users 200 --workers 50

前置：服务端已启动，库存足够的场次 id 已知（可用 --ticket 指定）。
建议把 config.json 的 payment.settle_delay_ms 调小（如 500）以加快结算。
退出码非 0 表示存在一致性违规。
"""
import argparse
import json
import socket
import sys
import threading
import time
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed


class Client:
    """一条短连接，串行 send/recv 一行 JSON。"""

    def __init__(self, host, port, timeout=15.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.buf = b""

    def call(self, obj):
        self.sock.sendall((json.dumps(obj) + "\n").encode())
        while b"\n" not in self.buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("server closed")
            self.buf += chunk
        line, _, self.buf = self.buf.partition(b"\n")
        return json.loads(line.decode())

    def close(self):
        try:
            self.sock.close()
        except OSError:
            pass


def register_and_login(host, port, tel, pwd="Passw0rd!"):
    """注册（幂等，已存在则忽略错误）并登录，返回 token。"""
    c = Client(host, port)
    try:
        c.call({"type": 2, "usertel": tel, "passward": pwd, "username": "st_" + tel[-6:]})
        resp = c.call({"type": 1, "usertel": tel, "passward": pwd})
        if resp.get("status") != "OK":
            return None
        return resp.get("token")
    finally:
        c.close()


def user_flow(host, port, token, ticket_id, pay_attempts):
    """单用户：下单 → 并发多次发起支付（测幂等）→ 轮询到终态。"""
    result = {"ordered": False, "final": None, "amounts": set(), "pay_nos": set()}
    c = Client(host, port)
    try:
        request_id = f"stress-{time.time_ns()}-{threading.get_ident()}"
        order = c.call({"type": 5, "token": token, "index": ticket_id,
                        "quantity": 1, "request_id": request_id})
        if order.get("status") != "OK":
            return result  # 无票/下单失败
        deadline = time.time() + 15
        resv_id = None
        while time.time() < deadline:
            queued = c.call({"type": 25, "token": token, "request_id": request_id})
            if queued.get("order_status") == "PENDING":
                resv_id = queued.get("reservation_id")
                break
            if queued.get("order_status") == "FAILED":
                return result
            time.sleep(0.1)
        if not resv_id:
            return result
        result["ordered"] = True
        idempotency_key = f"stress-pay-{resv_id}"

        # 并发对同一订单发起 N 次支付，验证只建一笔流水
        def fire_pay():
            cc = Client(host, port)
            try:
                return cc.call({"type": 20, "token": token, "index": str(resv_id),
                                "provider": "MOCK", "idempotency_key": idempotency_key})
            finally:
                cc.close()

        with ThreadPoolExecutor(max_workers=pay_attempts) as ex:
            for r in [f.result() for f in [ex.submit(fire_pay) for _ in range(pay_attempts)]]:
                if r.get("status") == "OK":
                    if "amount" in r:
                        result["amounts"].add(r["amount"])
                    if r.get("payment_no"):
                        result["pay_nos"].add(r["payment_no"])

        # 轮询到终态
        deadline = time.time() + 15
        while time.time() < deadline:
            q = c.call({"type": 24, "token": token, "index": str(resv_id)})
            st = q.get("payment_status")
            if st and st not in ("CREATED", "PROCESSING", "REFUNDING"):
                result["final"] = st
                if q.get("payment_no"):
                    result["pay_nos"].add(q["payment_no"])
                break
            time.sleep(0.3)
        return result
    except (ConnectionError, socket.timeout, json.JSONDecodeError):
        return result
    finally:
        c.close()


def get_available(host, port, ticket_id):
    """从票务列表读取指定场次剩余库存。"""
    c = Client(host, port)
    try:
        resp = c.call({"type": 4})
        for t in resp.get("arr", []):
            if str(t.get("index") or t.get("id") or t.get("tk_id")) == str(ticket_id):
                for k in ("num", "available_seats", "remain"):
                    if k in t:
                        return int(t[k])
        return None
    finally:
        c.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=7000)
    ap.add_argument("--ticket", type=int, required=True, help="目标场次 id")
    ap.add_argument("--users", type=int, default=200, help="并发用户数")
    ap.add_argument("--workers", type=int, default=50, help="线程池大小")
    ap.add_argument("--pay-attempts", type=int, default=3, help="每单并发发起支付次数（测幂等）")
    args = ap.parse_args()

    before = get_available(args.host, args.port, args.ticket)
    print(f"[start] ticket={args.ticket} available_before={before}")

    print(f"[setup] registering/login {args.users} users ...")
    tokens = []
    with ThreadPoolExecutor(max_workers=args.workers) as ex:
        futs = [ex.submit(register_and_login, args.host, args.port, f"139{i:08d}")
                for i in range(args.users)]
        for f in as_completed(futs):
            tok = f.result()
            if tok:
                tokens.append(tok)
    print(f"[setup] {len(tokens)} tokens ready")

    t0 = time.time()
    results = []
    with ThreadPoolExecutor(max_workers=args.workers) as ex:
        futs = [ex.submit(user_flow, args.host, args.port, tok, args.ticket, args.pay_attempts)
                for tok in tokens]
        for f in as_completed(futs):
            results.append(f.result())
    elapsed = time.time() - t0

    ordered = sum(1 for r in results if r["ordered"])
    finals = Counter(r["final"] for r in results if r["final"])
    success = finals.get("SUCCEEDED", 0)
    multi_amount = [r for r in results if len(r["amounts"]) > 1]
    multi_payno = [r for r in results if len(r["pay_nos"]) > 1]

    after = get_available(args.host, args.port, args.ticket)

    print("\n==== 结果 ====")
    print(f"耗时: {elapsed:.2f}s  QPS(下单): {ordered/elapsed:.0f}")
    print(f"下单成功: {ordered}  支付终态分布: {dict(finals)}")
    print(f"available_before={before}  available_after={after}  success={success}")

    ok = True
    # 幂等：同一订单不应出现多个金额或多个支付单号
    if multi_amount:
        print(f"[FAIL] {len(multi_amount)} 个订单出现多个支付金额（重复扣款）")
        ok = False
    if multi_payno:
        print(f"[FAIL] {len(multi_payno)} 个订单出现多个支付单号（重复建流水）")
        ok = False
    # 不超卖 + 库存守恒
    if before is not None and after is not None:
        if success > before:
            print(f"[FAIL] 超卖：成功支付 {success} > 初始库存 {before}")
            ok = False
        if success + after != before:
            print(f"[FAIL] 库存不守恒：success({success}) + after({after}) != before({before})")
            ok = False

    if ok:
        print("[PASS] 无超卖、支付幂等、库存守恒")
    print("==============")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
