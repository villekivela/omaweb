# Build the aarch64 package on Arch Linux ARM

Omaweb publishes an `aarch64` package, built on a GitHub `ubuntu-24.04-arm` runner in a community
Arch Linux ARM container image pinned by digest. `packaging/PKGBUILD` has declared
`arch=('x86_64' 'aarch64')` throughout and a source build on `aarch64` has always worked; what was
missing was a machine to build it on.

There is no official Arch container for `arm64`. Arch Linux ARM is a separate distribution with its
own package set, its own keyring and its own cadence, and the `archlinux` image is published for
`linux/amd64` alone. So the choice was between an image the Arch project does not publish and no
`aarch64` package at all. Building the existing image under QEMU was measured and is not a third
option: this build under emulation runs far past any timeout a release job can be given.

Since [ADR 0043](0043-serve-upgrades-from-a-signed-pacman-repository.md) that gap is no longer one
build behind. `Server = https://villekivela.github.io/omaweb/$arch` resolves the architecture, so an
`aarch64` reader whose machine asks for a directory nothing was ever published to cannot sync the
repository at all. That is not one version behind; it is outside the upgrade path, and the machine
Omaweb is developed on is one of the machines there. An unofficial base image in the release path is
a smaller thing to carry than an upgrade path a whole architecture cannot use.

The image is pinned by digest, which the official one is not. A distribution publishes its own image
and a community rebuild is one person's account; the digest is what makes the thing that runs on
release day the thing that was looked at. It goes stale, and refreshing it is reading the dated tag
in the comment beside it and taking a newer one. The alternative, following a floating tag, is
trusting every future push sight unseen in the job that signs packages, which is the wrong job to be
relaxed in.

The Arm leg installs its toolchain with pacman's own sandbox off. pacman 7 downloads inside a
Landlock ruleset, that ruleset cannot be applied on the Arm runner, and the failure is a job that
cannot synchronise a package database at all. The sandbox is there for a download running as root on
a machine that keeps its filesystem; this one runs in a container that exists for one job. Nothing
about what is installed changes, because the packages are still the distribution's own signed ones
and pacman still verifies them.

What the two packages are is not identical, and the difference is the engine. Omaweb depends on
system `qt6-webengine` rather than bundling it
([ADR 0013](0013-preserve-engine-sandboxes-in-every-build.md)), so the `aarch64` package is built
against, and runs against, the QtWebEngine Arch Linux ARM ships, which that distribution rebuilds on
its own schedule. `security/baseline.json` records one approved engine version, and Settings reports
the engine a build is actually running, so an `aarch64` reader whose distribution is behind Arch is
told so by the browser in the same words an `x86_64` reader is. The baseline is a claim about the
engine, not about who packaged it, and it stays one baseline: a second number per architecture would
be two claims to keep current and a reader could not tell which one the browser was reporting
against.

The release workflow builds the two packages as a matrix and neither publishes without the other. A
tag that built only `x86_64` would serve an upgrade to some readers and nothing to the rest, which
is the state this decision exists to end; if the `aarch64` leg fails, the fix is to make it build,
and the tag can be pushed again. The inventory is generated once, in a job of its own, because what
it records is read out of this repository and does not depend on the machine that built a package.

The `aarch64` leg runs on a runner and an image no other workflow uses, so `release.yml` also
answers to `workflow_dispatch`: a run builds both packages, attaches them to the run, and stops
before anything is published. That is the rule in `docs/agents/code-style.md` applied to a release
path: the first run of this leg is a dispatch, not a tag. GitHub accepts a dispatch only for a
workflow whose default-branch copy declares the trigger, so the first dispatch is a run on `main`
after this merges rather than one from the branch that wrote it, and it is still a run that
publishes nothing.
