---
name: omaweb
description:
  Drive the reader's Omaweb browser with the `omaweb` CLI. Use when a task needs a real browser,
  such as checking a page the project serves, filling a form, reading a site, taking a screenshot,
  or finding a page's console errors.
---

# Omaweb

`omaweb` talks to the browser the reader already runs. Each command prints plain text and exits: 0
done, 1 refused (the reason is on stderr), 2 a malformed command, 3 no browser running. Only the
reader can start the browser or turn on Allow agents; ask them when a command says either is
missing.

## The loop

Page commands work in an Agent Space. Make one for the task and pass its id, which `space new`
prints, on the first `open`:

```sh
omaweb space new signup-check
omaweb open http://localhost:3000/signup --space <id>
omaweb look
omaweb do 'fill 1 reader@example.com' 'fill 2 "A Reader"' 'click 5'
omaweb space delete <id>
```

1. `open` loads the address and makes its tab current. Later commands use the current tab.
2. `look` prints the title, the address, a short outline and one line per target in view:
   `[5] button "Sign up"`. The number is the target's label, and it stays the same for as long as
   the document does, so look again only after a navigation or when the page has changed.
3. `do` runs all the steps you give it in one call, waits for the page to settle after each, stops
   at the first that fails, and prints what each step did followed by a fresh `look`. Batch every
   step you can already see a label for.
4. Read the `look` that `do` printed and decide the next batch.
5. Delete the Space when the task is done.

## Commands

```sh
omaweb spaces
omaweb tabs [--space <id|name>]
omaweb open <address> [--space <id|name> | --tab <id>] [--new]
omaweb close [--tab <id>]
omaweb space new [name]
omaweb space delete <id|name>
omaweb look [--all]
omaweb read [selector]
omaweb do <step>... [--settle <ms>] [--timeout <ms>]
omaweb shot [--full] [--output <file>]
omaweb eval <expression>
omaweb console [--level error|warning|all] [--since <cursor>]
```

- Steps: `click <label>`, `fill <label> <text>`, `press <key>` (`Enter`, `Control+a`),
  `select <label> <option>`, `scroll <label|up|down|top|bottom>`, `back`, `wait text <text>`,
  `wait url <address>`. Give each step as one argument, or several in one argument separated by `;`.
  Quote a text to keep its spaces.
- `look --all` includes targets outside the viewport.
- `read` prints the page, or what a CSS selector matches, as Markdown. Use it for text; use `look`
  for what can be acted on.
- `shot` prints the path of the PNG it wrote. Open that file to see the page.
- `console` prints level, source and text, one message a line, then `cursor <n>`. Pass that number
  as `--since` to get only newer messages.
- `eval` runs JavaScript in an isolated world: it sees the DOM, not the page's own variables.
- `--tab <id>` points any page command at another tab. `--json` prints the browser's answer as JSON.
