#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
env_file="$project_dir/.env"
test_password="Phase4Test1"
test_provider="${PAYMENT_E2E_PROVIDER:-MOCK}"
seat_mode="${PAYMENT_E2E_SEAT_MODE:-false}"
expect_unavailable="${PAYMENT_E2E_EXPECT_ALIPAY_UNAVAILABLE:-false}"
case "$test_provider" in
  MOCK) test_tel="13999990004" ;;
  ALIPAY) test_tel="13999990005" ;;
  WECHAT) test_tel="13999990006" ;;
  *) echo "Unsupported PAYMENT_E2E_PROVIDER: $test_provider" >&2; exit 2 ;;
esac

read_env() {
  local key="$1"
  sed -n "s/^${key}=//p" "$env_file" | tail -n 1 | tr -d '\r'
}

db_host="$(read_env DB_HOST)"
db_port="$(read_env DB_PORT)"
db_user="$(read_env DB_USER)"
db_name="$(read_env DB_NAME)"
export MYSQL_PWD="$(read_env DB_PASSWORD)"

legacy_hash="$(python3 - "$test_password" <<'PY'
import sys
value = 1469598103934665603
for byte in sys.argv[1].encode():
    value ^= byte
    value = (value * 1099511628211) & ((1 << 64) - 1)
print(format(value, "x"))
PY
)"

mysql_cmd=(mysql --protocol=tcp -h "$db_host" -P "$db_port" -u "$db_user" "$db_name")
cleanup() {
  user_id="$("${mysql_cmd[@]}" -Nse "SELECT id FROM users WHERE tel='$test_tel'")"
  if [[ -n "$user_id" ]]; then
    affected_tickets="$("${mysql_cmd[@]}" -Nse \
      "SELECT DISTINCT ticket_id FROM reservations WHERE user_id=$user_id")"
    "${mysql_cmd[@]}" -e "
      UPDATE tickets t JOIN (
        SELECT ticket_id,SUM(quantity) quantity FROM reservations
        WHERE user_id=$user_id AND status IN ('PENDING','CONFIRMED') GROUP BY ticket_id
      ) r ON r.ticket_id=t.id
      SET t.available_seats=LEAST(t.total_seats,t.available_seats+r.quantity);
      UPDATE seats s JOIN reservations r ON r.id=s.reservation_id
      SET s.status='AVAILABLE',s.reservation_id=NULL WHERE r.user_id=$user_id;
      DELETE FROM reservations WHERE user_id=$user_id;
      DELETE FROM users WHERE id=$user_id;" >/dev/null
    while IFS= read -r affected_ticket; do
      [[ -z "$affected_ticket" ]] && continue
      available="$("${mysql_cmd[@]}" -Nse \
        "SELECT available_seats FROM tickets WHERE id=$affected_ticket")"
      redis_host="$(read_env REDIS_HOST)"; redis_host="${redis_host:-127.0.0.1}"
      redis_port="$(read_env REDIS_PORT)"; redis_port="${redis_port:-6379}"
      redis-cli -h "$redis_host" -p "$redis_port" \
        SET "stock:$affected_ticket" "$available" >/dev/null
    done <<< "$affected_tickets"
  fi
}
cleanup
"${mysql_cmd[@]}" -e "
  INSERT INTO users(tel,username,password_hash,status,phone_verified_at)
  VALUES('$test_tel','phase4_e2e','$legacy_hash',1,NOW(3));"
trap cleanup EXIT

if [[ "$seat_mode" == "true" ]]; then
  ticket_id="$("${mysql_cmd[@]}" -Nse \
    "SELECT t.id FROM tickets t JOIN seats s ON s.ticket_id=t.id
     WHERE t.status=1 AND t.available_seats>=2 AND s.status='AVAILABLE'
     GROUP BY t.id ORDER BY t.id LIMIT 1")"
else
  ticket_id="$("${mysql_cmd[@]}" -Nse \
    "SELECT id FROM tickets WHERE status=1 AND available_seats>=2 AND price>0 ORDER BY id LIMIT 1")"
fi
if [[ -z "$ticket_id" ]]; then
  echo "No active ticket with at least two available seats." >&2
  exit 1
fi

extra_args=()
if [[ "$seat_mode" == "true" ]]; then extra_args+=(--seat-mode); fi
if [[ "$expect_unavailable" == "true" ]]; then extra_args+=(--expect-alipay-unavailable); fi
python3 "$project_dir/tests/payment_e2e.py" \
  --host 127.0.0.1 --port 7000 --tel "$test_tel" \
  --password "$test_password" --ticket "$ticket_id" --provider "$test_provider" \
  "${extra_args[@]}"
