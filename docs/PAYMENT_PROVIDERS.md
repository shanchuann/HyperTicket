# Payment Providers

HyperTicket routes each payment and refund through the Provider recorded on the
payment row. The application currently contains:

- `MOCK`: deterministic automation Provider.
- `ALIPAY`: development-only simulated placeholder.
- `WECHAT`: development-only simulated placeholder.

The two simulated channel Providers never contact Alipay or WeChat, never move
real funds, and must not be presented as real payment support.

## Enable placeholders

They are disabled by default. For local development only, set:

```dotenv
HYPERTICKET_PAYMENT_SIMULATED_CHANNELS_ENABLED=true
HYPERTICKET_PAYMENT_SIMULATED_WEBHOOK_SECRET=use-a-random-development-secret
```

When disabled, requests using these channels return
`PAYMENT_PROVIDER_UNAVAILABLE`. The webhook secret is loaded only from the
environment or `.env`; it is not stored in `config.json`.

The placeholders implement the complete `IPaymentProvider` contract:

- `createPayment`
- `queryPayment`
- `closePayment`
- `refundPayment`
- `verifyWebhook`

Settlement remains server-controlled. A client cannot submit a successful
payment result. Simulated query results are deterministic, and simulated
webhook signatures use HMAC-SHA256 solely for integration testing.

## Replace with a real channel

Keep the domain service and database state machine unchanged. Replace the
corresponding `SimulatedPaymentProvider` registration with a production
`IPaymentProvider` adapter that:

1. Uses the official channel SDK or HTTPS API.
2. Loads merchant credentials from a secret manager or environment.
3. Maps channel responses to `ProviderPaymentState` without trusting clients.
4. Verifies webhook signatures with the channel's official algorithm.
5. Uses the existing payment number as the merchant order number.
6. Makes create, close, query, and refund operations idempotent.

Do not enable a real adapter until merchant onboarding, callback URLs, TLS,
key rotation, and provider sandbox acceptance tests are complete.

Refund delivery is retried up to five times with exponential backoff. A user
may hide a cancelled order, but the reservation, payment, refund, and audit
rows remain available for operations and reconciliation.
