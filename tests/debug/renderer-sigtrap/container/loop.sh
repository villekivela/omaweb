#!/bin/bash
# Inside the container, as root: CI's measurement step, repeated by WORKERS cages.
cd /home/builder/src
LOOP_SOURCE=/home/builder/src PYTHONPATH=scripts python3 /work/bench.py --print-dns-zone \
    > /tmp/zone.conf
pgrep -x dnsmasq || dnsmasq --conf-file=/tmp/zone.conf --pid-file=/tmp/dnsmasq.pid
echo 'nameserver 127.0.0.1' > /etc/resolv.conf
for w in $(seq 0 $((${WORKERS:-3} - 1))); do
    su builder -c "
        export XDG_RUNTIME_DIR=/tmp/runtime-builder-$w WORKER=$w $LOOP_EXTRA
        mkdir -p \$XDG_RUNTIME_DIR
        chmod 700 \$XDG_RUNTIME_DIR
        export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1
        cage -- /work/inner.sh
    " > /work/cage-$w.log 2>&1 &
done
wait
