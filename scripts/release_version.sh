#!/usr/bin/env bash
#
# Says what a release run is building, so that every job in `release.yml`
# answers with the same version rather than each working it out again.
#
#     scripts/release_version.sh                  # 0.5.0
#     scripts/release_version.sh --source-fragment # tag=v0.5.0
#
# On a tag the answer is the tag, which is where CMake takes the version from
# (ADR 0028) and therefore the only answer a package may carry. A run with no
# tag is the dry run `workflow_dispatch` starts: it builds a commit under the
# name of the release before it and publishes nothing, so the version is the
# nearest release tag and the source fragment names the commit rather than a
# tag that does not describe it.
#
# The fragment is the part after `#` in the `git+file://...` source a PKGBUILD
# is given. It comes from here because it is the same question as the version:
# which commit is being built, and under what name.

set -euo pipefail

what="${1:---version}"

if [[ -n "${GITHUB_REF_TYPE:-}" && "${GITHUB_REF_TYPE}" == "tag" ]]; then
    tag="${GITHUB_REF_NAME:?GITHUB_REF_TYPE says tag and GITHUB_REF_NAME is empty}"
    fragment="tag=$tag"
else
    tag=$(git describe --tags --abbrev=0 --match 'v[0-9]*')
    # `GITHUB_SHA` rather than `HEAD`: a workflow checkout is detached at the
    # commit the run is for, and naming it is what lets the PKGBUILD's source
    # be this checkout without depending on which ref it was reached by.
    fragment="commit=${GITHUB_SHA:-$(git rev-parse HEAD)}"
fi

case "$what" in
    --version) printf '%s\n' "$tag" ;;
    --source-fragment) printf '%s\n' "$fragment" ;;
    *)
        echo "usage: $0 [--version | --source-fragment]" >&2
        exit 2
        ;;
esac
