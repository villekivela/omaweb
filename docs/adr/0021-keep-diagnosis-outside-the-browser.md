---
status: superseded by ADR-0027
---

# Keep diagnosis outside the browser

Omaweb collects, filters, and deterministically correlates browser evidence, but it does not embed an agent, choose a model, generate diagnostic explanations, hold provider credentials, or send page data to an AI service. The user launches any supported agent in an external terminal, and that agent consumes the same versioned Diagnostics contract as the human TUI through MCP or the CLI.

This keeps Omaweb's automatic network behavior and Account boundary intact, avoids tying Diagnostics to one agent provider, and leaves source editing, server processes, tests, and application logs with the user's normal development tools. Omaweb owns only the browser evidence and page control those tools otherwise lack.
