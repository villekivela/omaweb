#!/bin/sh
# Records the website's introductory film from the real browser, in CI's Arch container.
#
#   scripts/record_film.sh
#
# writes `omaweb.webm`, `omaweb.mp4`, `poster.webp` and `captions.vtt` to `build/film/`, from
# the commit checked out, and fails without writing them when a beat did not happen.
# `film/README.md` has what the film shows and how it reaches the website.
#
# On the host this needs only Docker. The commit is handed to the container as an archive, so
# the film is of what is committed rather than of whatever the working tree holds, and nothing in
# the checkout is built or owned by the container. `OMAWEB_FILM_IMAGE` picks the image, which is
# CI's `archlinux:base-devel` by default, and `OMAWEB_FILM_DOCKER` adds arguments to `docker run`,
# for example `--cpuset-cpus 0-5` on a machine whose memory a full-width build would exhaust.
set -eu

cd "$(dirname "$0")/.."

if [ "${1:-}" != "--inside" ]; then
    mkdir -p build/film
    # shellcheck disable=SC2086 # OMAWEB_FILM_DOCKER is a list of arguments.
    git archive --format=tar HEAD | docker run --rm -i ${OMAWEB_FILM_DOCKER:-} \
        -v "$PWD/build/film:/film" "${OMAWEB_FILM_IMAGE:-archlinux:base-devel}" \
        sh -c 'mkdir -p /src && tar -x -C /src && exec /src/scripts/record_film.sh --inside'
    ls -l build/film/omaweb.webm build/film/omaweb.mp4 build/film/poster.webp
    exit 0
fi

# Inside the container, as root.
#
# The build is CI's `arch-linux-budget` job, against Omaweb's own engine from the [omaweb]
# repository.
# `cage`, `wtype` and `qt6-wayland` are the runtime budget's compositor, keyboard and platform
# plugin; `wf-recorder` and `grim` record its output and sample it; `wlr-randr` sizes it; `mesa`
# is the GL the browser draws with; `ffmpeg` cuts and encodes the film. The fonts are the ones the
# chrome and the fixture sites ask for by name.
pacman -Syu --noconfirm git cmake ninja clang ccache rust qt6-base qt6-declarative \
    qt6-shadertools qt6-tools qt6-wayland libsecret libsodium cage wtype gnu-free-fonts mesa \
    wf-recorder grim wlr-randr ffmpeg python ttf-jetbrains-mono inter-font ttf-ibm-plex noto-fonts
scripts/trust_omaweb_repository.sh
pacman -Syu --noconfirm omaweb-qtwebengine

# Every fixture site on loopback. The browser resolves through the hosts file before DNS, and
# these names are `.test`, which nothing outside the container answers for.
for host in $(python3 -c 'import sys; sys.path.insert(0, "scripts"); import record_film
print(" ".join(record_film.SITES))'); do
    echo "127.0.0.1 $host" >> /etc/hosts
done

# Chromium refuses to start as root with its sandbox on, and Omaweb does not turn it off.
useradd --create-home --shell /bin/bash builder
chown -R builder /src
chmod 0777 /film
su builder -c 'scripts/bootstrap_content_blocker.sh'
# The film's build takes the one verb its recording needs, `film-hover`, which no shipped build has.
su builder -c 'cmake --preset ci -DQT_ADDITIONAL_PACKAGES_PREFIX_PATH=/usr/lib/omaweb \
    -DOMAWEB_FILM_HOOKS=ON'
scripts/check_omaweb_engine.sh build/ci
# The browser as the runtime budget builds it: since the client and the browser are two programs
# (#565), `omaweb` in the build tree is the client alone. The film's verb is tested here, in the
# one build that has it.
su builder -c 'cmake --build --preset ci --target omaweb-browser omaweb-translations \
    omaweb-agent-command-tests omaweb-agent-control-tests'
su builder -c "ctest --preset ci -R '^omaweb-agent-(command|control)$'"

# Cage runs the recording as its one program, as it runs the runtime budget, and the status is
# carried out through a file because the answer that matters is the script's, not cage's.
cat > /tmp/record.sh <<'SCRIPT'
#!/bin/sh
cd /src
python3 scripts/record_film.py record --browser build/ci/omaweb-browser --out /film
echo $? > /film/record.status
SCRIPT
chmod 0755 /tmp/record.sh
rm -f /film/record.status
su builder -c '
    set -eu
    export XDG_RUNTIME_DIR=/tmp/runtime-builder
    mkdir -p "$XDG_RUNTIME_DIR"
    chmod 700 "$XDG_RUNTIME_DIR"
    export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1
    cage -- /tmp/record.sh
'
status=$(cat /film/record.status 2>/dev/null || echo 1)
if [ "$status" != 0 ]; then
    echo "the recording failed; build/film/browser.log has the browser's side" >&2
    exit "$status"
fi
su builder -c 'python3 scripts/record_film.py compose --out /film'
