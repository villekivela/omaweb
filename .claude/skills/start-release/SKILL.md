---
name: start-release
description:
  Open the next Omaweb release for work. Use when the user asks to start, open, or plan the next
  release or milestone, or when a release has just been published and its milestone is still open.
---

# Starting a release

A release starts when its milestone is the one being worked and every issue in it can be picked up
without a question. The work here makes that true, then names the order the issues go in. It ends
before any issue is started.

Milestones, their order and the roadmap are described in `docs/agents/issue-tracker.md` under
Releases. Branching is `docs/agents/commits.md`.

## Steps

1. **Close the release that shipped.** Find the milestone for the last published tag:

   ```sh
   gh release list --limit 3
   gh api repos/villekivela/omaweb/milestones --jq '.[] | "\(.number) \(.title) \(.open_issues)"'
   ```

   Check the tag's `Release` run published, as `cut-release` describes under "Then watch it". A
   milestone with a failed run stays open. Otherwise move any issue still open in it to the next
   milestone or out of every milestone, with the user, and close the milestone:

   ```sh
   gh api -X PATCH repos/villekivela/omaweb/milestones/<number> -f state=closed
   ```

   Done when no milestone for a published tag is open.

2. **Pick the release.** It is the first open milestone in the order `docs/roadmap.md` gives. A
   patch milestone opened for a fix outranks it and is handled by `cut-release` once the fix merges,
   not here.

3. **Sweep the backlog into it.** List the open issues with no milestone:

   ```sh
   gh issue list --state open --search "no:milestone" --limit 200 \
       --json number,title,labels --jq '.[] | "#\(.number) \(.title) \([.labels[].name] | join(","))"'
   ```

   For each, propose a milestone or a reason to leave it out: waiting on upstream, deferred in the
   roadmap, still an investigation, or settled by another issue. The user decides. Done when every
   issue on the list has an answer.

4. **Make every issue ready.** Each issue in the release carries `ready-for-agent` or
   `ready-for-human`. Take a `needs-triage` issue through triage with the user. A `needs-info` issue
   leaves the release until the information arrives, and an issue with no triage label is triaged
   like a `needs-triage` one. A `ready-for-agent` issue can still carry questions in its body; read
   its comments for the answers. Done when this prints nothing:

   ```sh
   gh issue list --milestone <version> --state open --json number,labels \
       --jq '.[] | select([.labels[].name] | any(startswith("ready-for-")) | not) | .number'
   ```

5. **Link the blockers.** Read each issue for work it needs from another, and record it as a native
   dependency, as `docs/agents/issue-tracker.md` describes under Wayfinding operations. Three kinds
   recur:
   - **Engine patches.** An issue that changes the QtWebEngine patch series needs the series
     qualified (`engine-release`), built on the rented builders and published before any browser
     release can depend on it. That is hours of building per architecture, so these start first.
   - **Decisions.** An issue that settles a question, such as how something is stored, blocks the
     issues that build on the answer.
   - **Shared surfaces.** Two issues editing the same settings section or the same model go one
     after the other, and the second names the first.

6. **Update the roadmap.** `docs/roadmap.md`'s Planned section names the release in progress and
   drops the one that shipped. Commit it on a `docs/` branch and open a pull request.

## Report

Give the user the release, its issues in the order they start, with each blocker named, and the
first issue that nothing blocks. Name anything left open in steps 1 and 3.

## While the release is in progress

- Each issue is a branch from `main`, named and merged as `docs/agents/commits.md` says.
- A new issue joins the release only when the user says so. The milestone description and the
  roadmap change with it.
- A fix that cannot wait for the release ships as a patch from a maintenance branch once the
  release's first `feat` has merged, and from `main` before that.
