#!/bin/sh
#
# Whether a push leaves the website as it was, so Vercel need not build a
# preview of it. `ignoreCommand` in `website/vercel.json` runs this from
# `website/` before every build, and Vercel's answer is inverted from the usual
# one: exit 0 skips the build and exit 1 runs it.
#
# Most pushes are to branches that change the browser and not the site, and
# each used to build a preview identical to the last. A skipped build still
# counts as a deployment, but it spends no build time.
#
# The site is `website/` and the files outside it that `SHARED` in
# `website/build/site.mjs` serves. A change to any of them builds.
#
# Every doubt builds. Production always does: the release pages come from
# GitHub's releases, so a release changes the site without changing a file in
# it. A preview is compared with the last commit Vercel built for its branch,
# or for a branch's first push with where it left `main`, and a base that
# cannot be read is a build. Building a preview nobody needed costs seconds;
# skipping one that was needed leaves a pull request without its preview.
#
#     sh ../scripts/website_unchanged.sh    # from website/, as Vercel runs it

set -u

readonly site='^website/|^share/scenes/crt-road\.json$|^scripts/install\.sh$'

build() {
    echo "website_unchanged: building, $1"
    exit 1
}

if [ "${VERCEL_ENV:-}" = production ]; then
    build "because production always builds"
fi

# Vercel clones ten commits deep, so the last built commit may be older than the
# clone, and a branch's first push has none.
base="${VERCEL_GIT_PREVIOUS_SHA:-}"
if [ -z "$base" ] || ! git cat-file -e "${base}^{commit}" 2>/dev/null; then
    git fetch --quiet --depth=50 origin main 2>/dev/null ||
        build "because main could not be fetched to compare with"
    base=$(git merge-base FETCH_HEAD HEAD 2>/dev/null) ||
        build "because the branch and main share no commit in the clone"
fi

changed=$(git diff --name-only "$base" HEAD) ||
    build "because the change since ${base} could not be read"
site_files=$(printf '%s\n' "$changed" | grep -E "$site")

if [ -n "$site_files" ]; then
    build "for:
$(printf '%s\n' "$site_files" | sed 's/^/  /')"
fi

echo "website_unchanged: nothing the site serves changed since ${base}, so no build"
exit 0
