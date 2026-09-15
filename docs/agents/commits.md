# Branches and commit messages

## Branch names

Work on a branch named for the change and the ticket it answers:

```text
<type>/<issue>-<summary>
```

Use the same types as the commit subject. Keep the summary imperative, lowercase, and hyphenated,
and drop the issue number only when there is no ticket. Examples: `fix/108-follow-the-editor-theme`,
`perf/70-step-the-sidebar-seam`.

## Commit messages

Use Conventional Commits for every new commit:

```text
<type>[optional scope][!]: <summary> [optional issue]
```

Allowed types are `feat`, `fix`, `test`, `refactor`, `docs`, `chore`, `build`, `ci`, `perf`, and
`revert`.

Keep the summary imperative and lowercase. Do not end it with a period. Keep the whole subject at 72
characters or fewer. Put an issue reference at the end as `(#123)` when it helps connect the commit
to a ticket.

Examples:

```text
feat(tabs): add keyboard link hints (#6)
fix: finish schema query before legacy migration
test: cover space restoration after restart
```

CI checks every commit after `35e01d4`. Earlier commits are retained as historical exceptions. Three
published commits after the baseline exceed the 72-character limit and are exempt by full hash in
`scripts/check_commit_messages.sh`.

Merge commits are not checked. GitHub generates their subject, and the convention governs the
commits an author writes.

## Merging

Squash every pull request. One pull request is one commit on `main`, whatever it took to get there.

The commits on a branch are a record of the work, including the parts that were wrong on the way.
`main` is read by someone asking what changed and by `scripts/release_notes.sh`, which lists every
non-merge commit in the range, so a feature that took six attempts arrives in the release notes as
six entries and a reader has to work out that five of them repair the first. Squashing puts that
history where it belongs, on the branch and in the pull request, and leaves `main` with the change
itself.

GitHub fills the squash commit's subject from the pull request title and appends ` (#123)` to it, so
a title has 64 characters rather than the subject's 72, and it has to follow the convention above.
CI checks the commit, not the pull request, so a title that is too long or written as a sentence
fails on the branch it lands on rather than the one it came from. Check one before opening a pull
request:

```sh
scripts/check_commit_messages.sh --pr-title "fix(ui): keep the divider off the page"
```

The pull request's description becomes the commit body. Write it as one: prose, wrapped, saying why
the change is what it is. Headings and checklists read as a review aid and land in `main` as
punctuation.
