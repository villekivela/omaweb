# Documentation style

Use these rules for Markdown files and code comments.

## Source layout

- Wrap prose at 100 characters.
- Keep headings, fenced code, Markdown tables, and unbreakable links intact when wrapping would
  change their meaning or rendering.
- Use one blank line around headings, lists, code blocks, and paragraphs.
- Use ATX headings with `#` and sentence case.
- Use `-` for unordered lists. Use `1.` for each item in a list whose numbering Markdown can supply.
- Add a language to every fenced code block when one applies.
- Use relative links for files in this repository.

## Prose

- State concrete behavior, constraints, and decisions. Cut introductions and conclusions that add no
  information.
- Prefer short active sentences and plain words.
- Use the repository's domain terms from `CONTEXT.md` consistently.
- Use straight quotes. Prefer commas or separate sentences to dash punctuation.
- Keep each document's detail appropriate to its purpose. Requirements state behavior. Architecture
  explains boundaries and tradeoffs. Development docs give commands and operational constraints.

## Comments

- Explain a constraint, tradeoff, invariant, or surprising dependency.
- Remove comments that repeat a name, statement, assertion, or control flow.
- Put API contracts in headers. Put implementation rationale beside the affected code.
- Wrap comments at the 100-character target unless the formatter must preserve a longer line.

Prettier enforces the source layout. Review remains responsible for meaning and voice.
