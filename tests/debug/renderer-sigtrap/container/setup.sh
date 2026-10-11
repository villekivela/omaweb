#!/bin/bash
# Inside the container, as root: CI's budget job up to the build.
set -eux
pacman -Syu --noconfirm git cmake ninja clang ccache qt6-base qt6-declarative qt6-shadertools \
    qt6-tools qt6-wayland libsecret libsodium cage wtype dnsmasq gnu-free-fonts procps-ng gdb
useradd --create-home --shell /bin/bash builder
mkdir -p /home/builder/src
tar -x -C /home/builder/src -f /work/src.tar
cd /home/builder/src
scripts/trust_omaweb_repository.sh
pacman -Syu --noconfirm omaweb-qtwebengine
pacman -Q omaweb-qtwebengine qt6-base mesa
blocker=
if [ -d /work/content-blocker ]; then
    blocker=-DOMAWEB_CONTENT_BLOCKER_CACHE=/work/content-blocker
else
    pacman -S --noconfirm rust
    su builder -c 'cd ~/src && scripts/bootstrap_content_blocker.sh'
fi
gcc -O1 -g -shared -fPIC -o /work/trapreport.so /work/trapreport.c
chown -R builder /home/builder
su builder -c "cd ~/src && CC=clang CXX=clang++ cmake --preset ci \
    -DQT_ADDITIONAL_PACKAGES_PREFIX_PATH=/usr/lib/omaweb -DOMAWEB_KNOWN_EXTENSIONS=ON $blocker \
    && scripts/check_omaweb_engine.sh build/ci \
    && cmake --build --preset ci -j6 --target omaweb-browser omaweb-translations"
