# Domain docs

These rules tell engineering skills how to read this repository's domain documentation.

## Read before exploring

Read the following files when they exist:

- `CONTEXT.md` at the repository root.
- Relevant decisions under `docs/adr/`.

If a file does not exist, continue without reporting its absence or proposing that it be created.
The `/domain-modeling` skill creates these files when the project resolves domain terms or
architectural decisions.

## File structure

This repository uses a single-context layout:

```text
/
├── CONTEXT.md
├── docs/adr/
│   ├── 0001-example-decision.md
│   └── 0002-another-decision.md
└── src/
```

## Use the glossary vocabulary

Use the terms defined in `CONTEXT.md` when naming domain concepts in issues, proposals, hypotheses,
and tests. Do not substitute synonyms that the glossary explicitly rejects.

A missing term may mean either that the proposed language does not fit the project or that the
domain model has a genuine gap. Reconsider the term first, then record a gap for `/domain-modeling`
if needed.

## Report ADR conflicts

Call out any proposal that contradicts an existing ADR. Name the ADR and explain why the decision
may need reconsideration.
