#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
env_file="$project_dir/.env"
backup_dir="$project_dir/backups"
mkdir -p "$backup_dir"

read_env() { sed -n "s/^$1=//p" "$env_file" | tail -n 1 | tr -d '\r'; }
db_host="$(read_env DB_HOST)"; db_port="$(read_env DB_PORT)"; db_user="$(read_env DB_USER)"; db_name="$(read_env DB_NAME)"
export MYSQL_PWD="$(read_env DB_PASSWORD)"
stamp="$(date +%Y%m%d-%H%M%S)"
mysql_cmd=(mysql --protocol=tcp -h "$db_host" -P "$db_port" -u "$db_user" "$db_name")

tables=(tickets seats reservations reservation_audit payments payment_events refunds payment_webhook_events favorites events venues halls event_sessions ticket_tiers attendees reservation_attendees browsing_history sale_reminders sale_reminder_audit event_favorites)
existing=()
for table in "${tables[@]}"; do
  if "${mysql_cmd[@]}" -Nse "SHOW TABLES LIKE '$table'" | grep -qx "$table"; then existing+=("$table"); fi
done
if ((${#existing[@]})); then
  mysqldump --protocol=tcp -h "$db_host" -P "$db_port" -u "$db_user" --single-transaction --no-create-info "$db_name" "${existing[@]}" | gzip > "$backup_dir/business-$stamp.sql.gz"
fi

"${mysql_cmd[@]}" < "$project_dir/db/migrate_v10_event_catalog.sql"
"${mysql_cmd[@]}" < "$project_dir/db/seed_v10_catalog.sql"
"${mysql_cmd[@]}" -Nse "SELECT CONCAT('events=',COUNT(*)) FROM events; SELECT CONCAT('sessions=',COUNT(*)) FROM event_sessions; SELECT CONCAT('tickets=',COUNT(*)) FROM tickets;"
echo "Backup: $backup_dir/business-$stamp.sql.gz"
