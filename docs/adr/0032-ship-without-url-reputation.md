# Ship without URL reputation

See #59 and [the provider research](../research/url-reputation-providers.md).

Omaweb will ship without a phishing, malware, or download-reputation provider. That omission does
not block daily-driver status. The browser documents the missing protection and never presents
Content blocking as equivalent protection. The privacy-focused Helium fork makes the same trade-off
by setting Chromium's `safe_browsing_mode` to zero. Omaweb avoids a credential service, recurring
cost, and provider terms that a small project cannot support.
[Helium build flags](https://github.com/imputnet/helium/blob/main/flags.gn)

## Consequences

- Content blocking may refuse some known malicious addresses, but Omaweb does not claim complete
  phishing, malware, or download-reputation coverage.
- Security updates, sandbox verification, download hardening, and Linux release qualification remain
  part of the daily-driver contract.
- Omaweb ships no provider credential, reputation database, threat interstitial, or automatic
  reputation request.
- Adding URL reputation later requires a new decision that names the provider, permitted credential
  model, privacy contract, and operating cost.

## Reconsider when

Reconsider this decision if a provider permits public open-source clients and offers a credential
model that does not require Omaweb to operate a paid service or conceal a secret in its binaries.
