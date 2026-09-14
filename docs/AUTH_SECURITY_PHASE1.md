# Authentication security phase 1

## Implemented

- `EXIT` now requires either `token` or `admin_token` and revokes that Redis session.
- User sessions maintain a `sessions:user:{user_id}` reverse index so password-reset code can revoke every device.
- Admin sessions use Redis, the same sliding TTL as user sessions, and an `admin_sessions:{username}` reverse index.
- A default-password admin session may only call `ADMIN_CHANGE_PASSWORD`; other admin operations return `ADMIN_PASSWORD_CHANGE_REQUIRED`.
- Changing an admin password revokes every existing admin session.
- Successful user/admin logins update `last_login`.
- Failed login throttles are shared in MySQL for account, source IP, and optional `client_id` scopes.
- Login success/failure, logout, and admin password changes are written to `security_audit`.
- The WebSocket bridge redacts password and token fields from logs.
- `deploy/nginx/hyperticket.conf.example` terminates public TLS before the loopback WebSocket bridge.

## Required migration

Run from WSL:

```bash
bash scripts/apply-auth-security-migration.sh
```

The migration is idempotent and creates `auth_throttles`, `security_audit`, and the admin login timestamp columns.

## Protocol notes

Clients should send a stable, random installation identifier as `client_id` during login. It is a rate-limit signal, not an authentication credential.

User logout:

```json
{"type":3,"token":"..."}
```

Admin logout:

```json
{"type":3,"admin_token":"..."}
```

After the configured failure threshold, login returns:

```json
{"status":"ERR","reason":"AUTH_TEMPORARILY_LOCKED","retry_after_seconds":899}
```

## Deployment boundary

The C++ TCP port and WebSocket bridge should remain bound to loopback or a private network. Only the TLS reverse proxy should be public. The bridge overwrites the internal client-IP field and redacts credentials before logging.
