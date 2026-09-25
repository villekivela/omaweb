# PROTOTYPE: an Agent driving Omaweb (#372)

Throwaway. It answers one question before ADR 0051 is accepted: is handing Omaweb to a coding agent
useful, and fast enough to reach for? It lives on `prototype/372-agent-mcp` and never merges.

It is a stdio MCP server with the proposed verbs (`tabs`, `open`, `look`, `do`, `read`, `shot`,
`console`, `eval`). It drives Omaweb through the `--remote-debugging` development launch, not the
socket, grants and Agent Spaces the ADR proposes.

## Limits of the prototype

- The debugging listener reaches every Space, and Private windows are off for that launch. Use a
  scratch data root, not your everyday Spaces, unless you accept that.
- Only a tab on show renders. Keep the Agent's tab on show, or beside your page in a split. A hidden
  tab falls back to synthetic clicks and is throttled.
- Omaweb cannot open a new tab through the listener. `open` navigates the current tab; open a tab
  yourself and `open` it by id to give the Agent a second one.
- The console is collected from when the prototype first uses a tab.

## Run it

```sh
# Omaweb with a scratch identity. Drop the two variables to use your own Spaces.
OMAWEB_DATA_ROOT=/tmp/omaweb-proto/data OMAWEB_CONFIG_ROOT=/tmp/omaweb-proto/config \
  omaweb --remote-debugging=9222

# Register once, in the project whose site you want the Agent to check.
claude mcp add omaweb-proto -- uv run --script /path/to/prototypes/372-agent-mcp/omaweb_agent_mcp.py
```

`OMAWEB_CDP` sets another port. Screenshots and the call log go to `$TMPDIR/omaweb-agent-prototype`.

## Trial

Use it for a few days on real work, then decide.

1. **Your dev site.** From a project that serves a site, ask the Agent why something on a page is as
   it is, or to check a change end to end. Note whether it reached for `console`, `read` and `do`
   and got an answer without you pasting anything.
2. **A logged-in chore.** Something you would otherwise click through yourself: triaging
   notifications, filling a form, collecting a few values from a dashboard.
3. **The same two tasks with chrome-devtools-mcp**, against Chrome, for comparison.

For each task note: wall time, whether it finished, how many tool calls, and whether you would have
been faster yourself. `uv run --script omaweb_agent_mcp.py --stats` prints calls, median latency and
approximate tokens per verb from the log.

The decision: accept ADR 0051 and start #375 and #376, change the design, or close #372.

## Measured on the fixture

macOS, dev build, a local sign-up page:

- The tool schema is about 670 tokens.
- `look` is about 110 tokens; the first call on a tab costs about 400 ms to attach, later ones a few
  milliseconds.
- One `do` that fills two fields, picks an option, ticks a box, submits and waits for the result
  took 1.2 s, and the page saw the click as `isTrusted`.
- `read`, `console` and `eval` answer in under 10 ms.
