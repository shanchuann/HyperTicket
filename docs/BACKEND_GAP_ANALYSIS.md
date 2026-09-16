# Backend Gap Analysis

This review compares HyperTicket with the backend capabilities normally found
in established ticketing platforms. It focuses on operational correctness and
does not claim parity with any specific company.

## Completed baseline

- Redis Streams order queue with atomic stock pre-deduction and idempotent DB persistence.
- Pending-order expiry, inventory compensation, and seat release.
- Redis sessions, login throttling, account lockout, OTP lifecycle, and password reset.
- Payment state machine, cents-based amounts, client idempotency, Provider routing,
  payment events, refund records, and simulated channel adapters.
- Retryable refunds with bounded exponential backoff.
- Order soft deletion that preserves payment, refund, and audit history.
- Redis-backed CI integration tests with an explicit health check.
- v10 activity, venue, hall, session, ticket-tier, and independent seat model.
- Multi-seat orders with attendee associations and rollback on persistence failure.
- User profiles, favorites, browsing history, common attendees, and electronic tickets.
- Sale-reminder subscriptions, scheduled email delivery, and reminder audit records.

## P0 before public sale traffic

1. **Traffic admission and anti-bot controls**
   Add per-event admission queues, signed sale tokens, device/account/IP quotas,
   CAPTCHA or risk challenges, and dynamic rate limits. A per-connection token
   bucket is not sufficient for high-demand public sales.

2. **Inventory reconciliation and recovery**
   Periodically compare ticket totals, reservations, seat rows, and Redis stock.
   Repair or quarantine mismatches and expose an operator-visible incident queue.

3. **Real webhook ingestion**
   Add an HTTPS-only callback endpoint, raw-body signature verification, event
   persistence before processing, duplicate/out-of-order handling, and replay
   protection. The table and Provider contract exist; the public ingress does not.

4. **Payment and refund recovery jobs**
   Add explicit operator states, alerts for exhausted refunds, provider query
   recovery after restart, timeout close jobs, and daily reconciliation files.
   Real network calls must use claim leases/outbox workers rather than holding
   MySQL row locks while waiting for a payment gateway.

5. **Order state machine enforcement**
   Move reservation transitions behind conditional repository methods and a
   documented state machine. Database ENUMs alone do not prevent every invalid
   transition or cross-process race.

## P1 operational maturity

1. Introduce structured audit correlation IDs across order, payment, refund,
   authentication, and administrator actions.
2. Add payment, refund, OTP, lockout, queue-lag, reconciliation, and inventory
   mismatch Prometheus metrics with bounded label cardinality.
3. Add transactional outbox events for notifications, ticket issuance, and
   downstream analytics instead of coupling them to request transactions.
4. Add event-level purchase limits and stronger attendee identity rules. Common
   attendees and seat associations exist; transfer policy, QR rotation, and
   check-in idempotency are still required.
5. Add MySQL backup/restore drills, Redis failover testing, schema migration CI,
   and restart recovery tests for every scheduled job.

## P2 product and compliance capabilities

- Pricing tiers, fees, promotions, tax/invoice data, and immutable price snapshots.
- Organizer settlement, ledger accounting, chargeback/dispute handling, and payout reports.
- Privacy retention policies, account anonymization, consent records, and regional compliance.
- Multi-region reads, disaster recovery targets, capacity planning, and sale-day runbooks.

The next recommended backend milestone is the P0 inventory reconciliation and
operator exception queue, followed by order-state transition enforcement. These
improve correctness without requiring payment merchant qualifications or
external platform contracts.
