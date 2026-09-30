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

A page command in one of the reader's own Spaces asks the reader to grant that Space and waits up to
a minute. Use one only when the task needs the reader's logins. After `denied` the reader is not
asked again for that Space, so work in an Agent Space instead.

## Commands

```sh
omaweb spaces
omaweb tabs [--space <id|name>]
omaweb open <address> [--space <id|name> | --tab <id>] [--new]
omaweb close [--tab <id>]
omaweb space <id|name>
omaweb focus <tab id|part of an address>
omaweb commands
omaweb run <command> [position]
omaweb space new [name] [--temporary]
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
  `wait url <address>`, `dialog accept [text]`, `dialog dismiss`, `upload <label> <file>...`. Give
  each step as one argument, or several in one argument separated by `;`. Quote a text to keep its
  spaces.
- A page's alert, confirm or prompt shows in `look` as `Dialog (confirm) "..."`, and the page is
  stopped until a `dialog` step answers it.
- A download lands in a directory of your own, and `look` prints its path. A High-risk download
  waits for the reader to confirm it.
- `upload` works only in an Agent Space, and gives the page only the files you name.
- A window the page opens prints as `Opened window-1`. Pass `--tab window-1` to use it.
- `look --all` includes targets outside the viewport.
- `read` prints the page, or what a CSS selector matches, as Markdown. Use it for text; use `look`
  for what can be acted on.
- `shot` prints the path of the PNG it wrote. Open that file to see the page.
- `console` prints level, source and text, one message a line, then `cursor <n>`. Pass that number
  as `--since` to get only newer messages.
- `eval` runs JavaScript in an isolated world: it sees the DOM, not the page's own variables.
- `space`, `focus` and `run` change what the reader sees: another Space, another tab, or a browser
  command such as `toggle-sidebar`. Use them only when the reader asks for that. `commands` lists
  what `run` can run now.
- `space new --temporary` keeps running after it prints the Space's id, and the Space is deleted
  when the process stops. Start it in the background and stop it when the task is done.
- Commands with the same `--name` share the current tab. The name defaults to that of the process
  that ran `omaweb`, so give it only when commands come from different processes.
- `--tab <id>` points any page command at another tab. `--json` prints the browser's answer as JSON.
