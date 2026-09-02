# Issue tracker: GitHub

Issues and specs for this repo live as GitHub issues. Use the `gh` CLI for all operations.

## Conventions

- Create an issue with `gh issue create --title "..." --body "..."`. Use a heredoc for multiline bodies.
- Read an issue with `gh issue view <number> --comments`. Fetch its labels and filter comments with `jq` when needed.
- List issues with `gh issue list --state open --json number,title,body,labels,comments --jq '[.[] | {number, title, body, labels: [.labels[].name], comments: [.comments[].body]}]'`. Add suitable `--label` and `--state` filters.
- Comment with `gh issue comment <number> --body "..."`.
- Apply or remove labels with `gh issue edit <number> --add-label "..."` and `gh issue edit <number> --remove-label "..."`.
- Close with `gh issue close <number> --comment "..."`.

The `gh` CLI infers `villekivela/omaweb` from the Git remote when run inside this repository.

## Pull requests as a triage request source

**PRs as a request surface: no.**

Set this to `yes` if the repo later treats external pull requests as feature requests. The `/triage` skill reads this flag.

When enabled, pull requests use the same labels and states as issues:

- Read a pull request with `gh pr view <number> --comments` and inspect its changes with `gh pr diff <number>`.
- List external pull requests with `gh pr list --state open --json number,title,body,labels,author,authorAssociation,comments`. Keep authors with an association of `CONTRIBUTOR`, `FIRST_TIME_CONTRIBUTOR`, or `NONE`.
- Comment, label, and close with `gh pr comment`, `gh pr edit`, and `gh pr close`.

GitHub shares one number sequence between issues and pull requests. Resolve an ambiguous reference such as `#42` with `gh pr view 42`, then fall back to `gh issue view 42`.

## Publishing to the issue tracker

When a skill says to publish to the issue tracker, create a GitHub issue.

## Fetching a ticket

When a skill says to fetch a ticket, run `gh issue view <number> --comments`.

## Wayfinding operations

The `/wayfinder` skill uses one issue as a map and links child issues as tickets.

- Map: an issue labelled `wayfinder:map` containing the Notes, Decisions-so-far, and Fog sections.
- Child ticket: a GitHub sub-issue linked to the map. If sub-issues are unavailable, add the child to a task list in the map and put `Part of #<map>` at the top of the child body. Apply a `wayfinder:<type>` label using `research`, `prototype`, `grilling`, or `task`.
- Blocking: use GitHub's native issue dependencies. Add an edge with `gh api --method POST repos/<owner>/<repo>/issues/<child>/dependencies/blocked_by -F issue_id=<blocker-db-id>`. Fetch the numeric database ID with `gh api repos/<owner>/<repo>/issues/<n> --jq .id`. If dependencies are unavailable, add `Blocked by: #<n>, #<n>` at the top of the child body.
- Frontier query: list the map's open children, remove assigned tickets and tickets with open blockers, then choose the first ticket in map order.
- Claim: run `gh issue edit <n> --add-assignee @me`.
- Resolve: comment with the answer, close the issue, and append a context pointer and link to the map's Decisions-so-far section.
