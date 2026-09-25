#!/bin/sh
#
# Publish the website as it stands at one commit, usually a release tag.
#
# Vercel builds the live site from the `website` branch, and only this moves it,
# so the site says what a reader can install rather than what has merged since.
# The push is what asks Vercel for the build. A branch already at the commit
# gets no push event, so the deploy hook in `HOOK` asks instead: a release's
# pages are generated from the releases API, and a release cut from the commit
# the site already shows still has to reach them.
#
# Pushes with --force because naming an older release is how a bad site is
# rolled back. `--dry-run` prints the commit and what would happen, and changes
# nothing.
#
#     HOOK=<deploy hook URL> scripts/publish_website.sh [--dry-run] <commit>

set -eu

dry_run=false
if [ "${1:-}" = "--dry-run" ]; then
    dry_run=true
    shift
fi
if [ $# -ne 1 ]; then
    echo "usage: $0 [--dry-run] <commit>" >&2
    exit 2
fi

commit=$(git rev-parse --verify --quiet "$1^{commit}") || {
    echo "$1 is not a commit here" >&2
    exit 2
}
published=$(git ls-remote origin refs/heads/website | cut -f1)

if [ "$published" != "$commit" ]; then
    echo "Publishing the website at $1 ($commit)"
    if [ "$dry_run" = false ]; then
        git push --quiet --force origin "$commit:refs/heads/website"
    fi
    exit 0
fi

echo "The website is already at $1 ($commit), so the deploy hook rebuilds it"
if [ "$dry_run" = true ]; then
    exit 0
fi
if [ -z "${HOOK:-}" ]; then
    echo "No VERCEL_DEPLOY_HOOK_URL secret: the website keeps what it last built"
    exit 0
fi
# The branch already says what the site should be, so a hook that does not
# answer leaves a page behind rather than a wrong one. It warns, and the caller
# carries on.
status=$(curl --silent --show-error --output /dev/null --write-out '%{http_code}' \
    --max-time 30 --request POST "$HOOK") || status="no answer"
echo "Vercel deploy hook answered $status"
case "$status" in
    2*) ;;
    *) echo "::warning::the website deploy hook answered $status, so the site is behind" ;;
esac
