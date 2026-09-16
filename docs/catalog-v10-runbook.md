# Catalog v10 runbook

## Apply the domain model

Run the migration and seed script from WSL2 after confirming the Windows MySQL instance is reachable:

```bash
./scripts/migrate-and-seed-v10.sh
```

The script writes a compressed business-data backup under `backups/` before clearing business-domain rows. It preserves users, administrators, verification state, sessions, and authentication security configuration. The seed aborts if user `13008569663` does not already exist.

The seed creates 35 fictional events across movie, concert, performance, stand-up comedy, exhibition, esports, and sports categories, with 70 sessions, venue coordinates, halls, ticket tiers, seats, demo history/favorites/reminders, and several order/payment states. All cover paths point to `clients/user-client/public/media/events`; attribution is in `clients/user-client/public/media/ATTRIBUTION.md`.

## Start the stack

```bash
./bin/ser
cd websocket-bridge && npm start
cd clients/user-client && npm run dev
cd clients/admin-client && npm run dev -- --port 5174
```

The C++ service continues to use Windows MySQL and WSL2 Redis. Public catalog requests tolerate an expired optional browser token and continue as a guest; authenticated actions remain strict.

## Admin workflow

The admin client now exposes Activity catalog tabs for activities, venues, halls, sessions, and ticket tiers, plus a sale-reminder audit table. Session creation validates `sale_start_at < sale_end_at < starts_at`, creates a compatibility ticket row and independent seat inventory, and supports copying a session configuration.

