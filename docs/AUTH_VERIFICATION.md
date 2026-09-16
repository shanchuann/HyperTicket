# Email verification, Mock SMS, and password reset

## Providers

- Email uses authenticated SMTP over certificate-verified implicit TLS.
- The sender header is `HyperTicket <configured-address>`.
- Mock SMS never contacts a carrier. It may return `mock_code` only when both
  `mock_sms_enabled` and `expose_mock_sms_code` are enabled in local config.
- Production-safe defaults disable Mock SMS and code exposure.

SMTP credentials belong in `.env`, never in Git. POP3, IMAP, Exchange, and
CardDAV are not used to send verification mail.

## Registration flow

1. Send a code:

```json
{"type":26,"usertel":"13800138000","email":"user@example.com","channel":"EMAIL","client_id":"installation-id"}
```

2. Verify the code and obtain a short-lived, single-use registration grant:

```json
{"type":27,"challenge_id":"...","code":"123456"}
```

3. Register with the grant:

```json
{"type":2,"usertel":"13800138000","username":"user","passward":"Password123","verification_token":"challenge.secret"}
```

For a local Mock SMS test, omit `email`, set `channel` to `SMS`, and use the
development-only `mock_code` from step 1.

## Password-reset flow

1. `type=28`: send `account`, `channel`, and `client_id`. The response is the
   same whether or not the account exists.
2. `type=29`: send `challenge_id` and the six-digit `code`; receive a short-lived
   `reset_token`.
3. `type=30`: send `reset_token` and `new_password`.

Successful reset consumes the token, updates the bcrypt hash, writes a security
audit event, and revokes every Redis session belonging to that user.

## Existing-account contact verification

Existing users can bind a verified recovery destination after login. These
endpoints require a valid user session; requesting a code also requires the
current password.

1. Read the current security status:

```json
{"type":31,"token":"..."}
```

2. Request an email binding code:

```json
{"type":32,"token":"...","passward":"CurrentPassword1","channel":"EMAIL","email":"user@example.com","client_id":"installation-id"}
```

For local Mock SMS, use `channel: "SMS"` and omit `email`. SMS verifies the
account's current login phone number; changing the primary login phone number is
intentionally not supported by this flow.

3. Confirm and bind the contact:

```json
{"type":33,"token":"...","challenge_id":"...","code":"123456"}
```

After confirmation, type 31 and subsequent login responses expose
`email_verified` and `phone_verified`. A verified destination can immediately
be used by the type 28-30 password-reset flow. Email addresses remain globally
unique, and a challenge belongs to the user session that requested it.

Protocol summary:

- `31 ACCOUNT_SECURITY_STATUS`
- `32 CONTACT_VERIFICATION_REQUEST`
- `33 CONTACT_VERIFICATION_CONFIRM`

## Security limits

- Code lifetime: 5 minutes.
- Verification/reset grant lifetime: 10 minutes.
- Maximum code attempts: 5.
- Resend cooldown: 60 seconds per destination/purpose/channel.
- Account, IP, device, and destination request limits are shared in MySQL.
- Passwords must be 8-64 characters and contain uppercase, lowercase, and digit.

## Database migration

```bash
bash scripts/apply-verification-migration.sh
```

The migration adds verified timestamps, a unique email index, and the
`auth_challenges` table. Verification codes and grant secrets are stored only as
bcrypt hashes.
