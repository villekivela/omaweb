# Serve upgrades from a signed pacman repository

Omaweb publishes its own pacman repository, and a reader who adds it gets new versions from
`pacman -Syu` along with the rest of their system. Before it, a release was a file: a reader
installed `v0.2.0` from an asset and nothing told them `v0.4.0` existed or brought it to their
machine. An alpha browser that releases often and ages quietly is worse for the reader than any of
the costs below.

The AUR is not the answer to this and was not weighed as one. It is not accepting new registrations,
and what it distributes is a PKGBUILD rather than a binary, so every reader would rebuild
QtWebEngine-linked code themselves. Nothing about an upgrade path needs it: a pacman repository is a
directory of packages and a database served over HTTP, and this project can serve its own.

The repository is hosted on GitHub Pages, from a `gh-pages` branch this repository owns. Pages
serves a fixed URL, which is what `Server =` needs; the release asset path carries the tag and
therefore changes every release, so it cannot be one. The branch is replaced by a single orphan
commit each time, because a branch that kept every package it ever served would grow by the size of
a browser at every release for a history nobody reads. For the same reason the repository holds one
version per architecture: the GitHub releases are this project's archive, and serving history here
would make the branch a second one that nobody maintains. Packages sit under a directory named for
their architecture from the first commit, so `aarch64`
([#185](https://github.com/villekivela/omaweb/issues/185)) arrives as a build rather than as a
change every reader has to make to a `pacman.conf` they already wrote.

The published package is `omaweb`, and `packaging/PKGBUILD` keeps building `omaweb-git` for a reader
building from a checkout. A `-git` package is one pacman expects to be rebuilt from source, and its
version comes from `git describe` in a tree a binary reader does not have; a binary package takes
the tag instead, the same tag CMake reads
([ADR 0028](0028-derive-the-version-from-the-release-tag.md)). The two are not two files.
`scripts/make_release_pkgbuild.sh` writes the binary one from the source one, rewriting the four
lines that differ and failing if any of them is not there to rewrite, because a second PKGBUILD
means a dependency added to one and forgotten in the other ships a package that does not run and
nothing reports the disagreement.

Packages and databases are signed, and pacman is configured to refuse anything unsigned. The key's
private half is a workflow secret and never in the repository; its fingerprint is published in the
README and on the website, in both because a reader installs from whichever of the two they opened,
and a keyserver will hand out any key that claims a name. The fingerprint is what makes
`pacman-key --recv-keys` worth running, so it has to come from somewhere the reader already decided
to trust. `scripts/check_repository_instructions.py` holds the two copies to the same value.

Omaweb never installs a release. pacman owns `/usr`, a browser that rewrites its own binary is a
security surface this project declines, and the same reasoning keeps the engine the distribution's
to update ([ADR 0013](0013-preserve-engine-sandboxes-in-every-build.md)). What the browser does is
say so: it asks GitHub once a day what the newest release is and marks one in the outline footer,
naming the version and the command that upgrades it, which is `pacman -Syu` for a package a
repository carries and something else for a package installed by hand or built from a checkout.

The publishing path is exercised on every change rather than on release day.
`scripts/check_repo_publish.sh` derives the binary PKGBUILD, signs a stand-in package with a
throwaway key, writes the databases, replaces a version, and installs from the result with a pacman
that trusts nothing else. The rule it answers to is the one in `docs/agents/code-style.md`: code
that calls something this repository does not run is exercised against the real thing before it
merges. Everything but the upload runs in CI, and the upload is the one step whose first execution
is a real release.
