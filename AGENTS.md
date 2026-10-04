## Agent skills

### Issue tracker

Issues and specs are tracked in GitHub Issues for `villekivela/omaweb`. See
`docs/agents/issue-tracker.md`.

### Triage labels

The repo uses the five default triage labels. See `docs/agents/triage-labels.md`.

### Domain docs

Domain documentation uses the single-context layout. See `docs/agents/domain.md`.

### Branches and commits

Before creating a branch or a commit, follow `docs/agents/commits.md`.

### Documentation and code quality

Before editing documentation or comments, read `docs/agents/documentation-style.md`. Before editing
code, build files, or automation, read `docs/agents/code-style.md`. After a tool call changes
documentation or code, run `scripts/format.sh` and inspect its diff. Before committing, run every
applicable quality gate in `docs/agents/code-style.md`.

### Local checks

Before formatting, building, testing or taking screenshots outside CI, read
`docs/agents/local-checks.md`: a check means what CI means only on CI's tools.

### Proving tests

Before opening a pull request, prove each test red as `docs/agents/proving-tests.md` describes.

### Omarchy and constraints

Before designing anything that talks to the desktop, read `docs/agents/omarchy.md`. Before proposing
an engine, rendering or interface change, read `docs/agents/constraints.md`.
