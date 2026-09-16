#!/usr/bin/env python3
import argparse
import json
import socket
import time


class Client:
    def __init__(self, host, port):
        self.socket = socket.create_connection((host, port), timeout=10)
        self.buffer = b""

    def call(self, payload):
        self.socket.sendall((json.dumps(payload) + "\n").encode())
        while b"\n" not in self.buffer:
            chunk = self.socket.recv(4096)
            if not chunk:
                raise RuntimeError("server closed connection")
            self.buffer += chunk
        line, self.buffer = self.buffer.split(b"\n", 1)
        return json.loads(line)

    def close(self):
        self.socket.close()


def require_ok(response, context):
    if response.get("status") != "OK":
        raise AssertionError(f"{context}: {response}")
    return response


def create_order(client, token, ticket_id, suffix, seat_id=None):
    request_id = f"phase4-order-{suffix}-{time.time_ns()}"
    payload = {
        "type": 5, "token": token, "index": ticket_id,
        "quantity": 1, "request_id": request_id,
    }
    if seat_id:
        payload["seat_id"] = seat_id
    require_ok(client.call(payload), "enqueue order")
    deadline = time.time() + 15
    while time.time() < deadline:
        response = require_ok(client.call({
            "type": 25, "token": token, "request_id": request_id,
        }), "query queued order")
        if response.get("order_status") == "PENDING":
            return response["reservation_id"]
        if response.get("order_status") == "FAILED":
            raise AssertionError(f"order failed: {response}")
        time.sleep(0.1)
    raise AssertionError("order did not become PENDING")


def wait_payment(client, token, reservation_id, expected, timeout=15):
    deadline = time.time() + timeout
    last = None
    while time.time() < deadline:
        last = require_ok(client.call({
            "type": 24, "token": token, "index": reservation_id,
        }), "query payment")
        if last.get("payment_status") == expected:
            return last
        time.sleep(0.2)
    raise AssertionError(f"payment did not become {expected}: {last}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=7000)
    parser.add_argument("--tel", required=True)
    parser.add_argument("--password", required=True)
    parser.add_argument("--ticket", type=int, required=True)
    parser.add_argument("--provider", default="MOCK", choices=("MOCK", "ALIPAY", "WECHAT"))
    parser.add_argument("--seat-mode", action="store_true")
    parser.add_argument("--expect-alipay-unavailable", action="store_true")
    args = parser.parse_args()

    client = Client(args.host, args.port)
    token = None
    reservations = []
    second_reservation = None
    try:
        login = require_ok(client.call({
            "type": 1, "usertel": args.tel, "passward": args.password,
            "client_id": "phase4-e2e",
        }), "login")
        token = login["token"]
        seat_id = None
        if args.seat_mode:
            seats = require_ok(client.call({
                "type": 17, "token": token, "index": args.ticket,
            }), "list seats")
            available = [row for row in seats.get("arr", []) if row.get("status") == "AVAILABLE"]
            if not available:
                raise AssertionError("no available seat for seat-mode test")
            seat_id = available[0]["id"]
        first_reservation = create_order(client, token, args.ticket, "a", seat_id)
        reservations.append(first_reservation)
        second_reservation = create_order(client, token, args.ticket, "b")
        reservations.append(second_reservation)
        key = f"phase4-pay-{first_reservation}"

        if args.expect_alipay_unavailable:
            unavailable = client.call({
                "type": 20, "token": token, "index": first_reservation,
                "provider": "ALIPAY", "idempotency_key": key,
            })
            assert unavailable.get("reason") == "PAYMENT_PROVIDER_UNAVAILABLE", unavailable

        created = require_ok(client.call({
            "type": 20, "token": token, "index": first_reservation,
            "provider": args.provider, "idempotency_key": key,
        }), "create payment")
        assert created["currency"] == "CNY" and created["amount_minor"] > 0, created

        duplicate = require_ok(client.call({
            "type": 20, "token": token, "index": first_reservation,
            "provider": args.provider, "idempotency_key": key,
        }), "repeat payment")
        assert duplicate["payment_no"] == created["payment_no"], duplicate

        parallel_key = client.call({
            "type": 20, "token": token, "index": first_reservation,
            "provider": args.provider, "idempotency_key": key + "-different",
        })
        assert parallel_key.get("reason") == "PAYMENT_IN_PROGRESS", parallel_key

        conflict = client.call({
            "type": 20, "token": token, "index": second_reservation,
            "provider": args.provider, "idempotency_key": key,
        })
        assert conflict.get("reason") == "PAYMENT_IDEMPOTENCY_CONFLICT", conflict

        settled = wait_payment(client, token, first_reservation, "SUCCEEDED")
        assert settled["order_status"] == "CONFIRMED", settled

        require_ok(client.call({
            "type": 7, "token": token, "index": first_reservation,
        }), "cancel paid order")
        refunded = wait_payment(client, token, first_reservation, "REFUNDED")
        assert refunded["order_status"] == "CANCELLED", refunded

        if seat_id:
            seats_after = require_ok(client.call({
                "type": 17, "token": token, "index": args.ticket,
            }), "list seats after cancellation")
            released = [row for row in seats_after.get("arr", [])
                        if row.get("id") == seat_id and row.get("status") == "AVAILABLE"]
            assert released, seats_after

        require_ok(client.call({
            "type": 16, "token": token, "index": first_reservation,
        }), "hide refunded order")
        orders = require_ok(client.call({
            "type": 6, "token": token,
        }), "list orders after hide")
        visible_ids = {str(row.get("reservation_id")) for row in orders.get("arr", [])}
        assert str(first_reservation) not in visible_ids, orders

        require_ok(client.call({
            "type": 7, "token": token, "index": second_reservation,
        }), "cancel pending order")
        print("payment_e2e: PASS")
    finally:
        if token:
            for reservation_id in reservations:
                try:
                    client.call({"type": 7, "token": token, "index": reservation_id})
                except Exception:
                    pass
        client.close()


if __name__ == "__main__":
    main()
