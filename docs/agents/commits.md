# Commit messages

Use Conventional Commits for every new commit:

```text
<type>[optional scope][!]: <summary> [optional issue]
```

Allowed types are `feat`, `fix`, `test`, `refactor`, `docs`, `chore`, `build`, `ci`, `perf`, and `revert`.

Keep the summary imperative and lowercase. Do not end it with a period. Keep the whole subject at 72 characters or fewer. Put an issue reference at the end as `(#123)` when it helps connect the commit to a ticket.

Examples:

```text
feat(tabs): add keyboard link hints (#6)
fix: finish schema query before legacy migration
test: cover space restoration after restart
```

CI checks every commit after `35e01d4`. Earlier commits are retained as historical
exceptions. Two published commits after the baseline also predate the 72-character
limit and are exempt by full hash in `scripts/check_commit_messages.sh`.
