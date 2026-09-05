---
status: superseded by ADR-0027
---

# Separate Diagnostics authorities

Agent access never becomes one unrestricted permission. Omaweb grants Observation access, Control
access, Destructive control, Sensitive-data access, Embedded-content access, and Main-world access
separately to each Diagnostics client; adding a client never copies another client's authority. One
client at a time exercises its Control access through a revocable Control lease. Omaweb pauses a
session when its tab reaches an unapproved top-level origin, requires separate approval for
Auxiliary windows and cross-origin frame contents, and never lets Diagnostics bypass browser-owned
permission, download, file, certificate, or credential prompts.

Remembered Observation and Control access is limited to exact origins and approved local-origin
patterns within one Space. Sensitive-data, Destructive-control, Embedded-content, and Main-world
access always expire with the session. Production and Private-window sessions require approval each
time, disappear when detached, and never survive a restart. A Project attachment may choose source
context for any associated origin, including production, but does not grant Diagnostics access.
