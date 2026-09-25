#!/bin/bash
#
# Build and run the suite on GNU/Hurd in a qemu VM (#1775). Driven by
# .github/workflows/hurd-suite.yml, which explains why a VM and not a
# cross-compile.

set -euo pipefail

# Pinned rather than the undated debian-hurd.img.tar.xz alias, so the image
# cannot move under a cached key. Checksum from the archive's own SHA512SUMS.
IMG_BASE=https://cdimage.debian.org/cdimage/ports/latest/hurd-amd64
IMG_NAME=debian-hurd-amd64-20260314.img.tar.xz
IMG_SHA512=0c6151e7a402c065ef337841a2586e5c6a9799955d4f25d454cb7c6e6d54e7e2d52768aea0874a794aa285089a085c3b99702de475bc2fc4f250a2ce5a9f6cf0

# The image ships 1.6G free on the root filesystem, against about 70M of tree
# and build plus the packages below. Growing it instead put the filesystem past
# what Hurd's own ext2fs can read, and the boot stopped at an I/O error.
NEED_KB=307200
# What the image's own README prescribes in every example. Hurd's SMP is young,
# and gnumach is not happy with more memory than this.
VM_CPUS=1
VM_MEM=2G
SSH_PORT=2222
# The unprivileged account the image ships, so the suite runs the way a buildd
# runs it.
VM_USER=demo
VM_HOME=/home/demo
# Hurd boots slower than Linux and the runner is shared, so this is generous.
BOOT_TIMEOUT=600
# A step that hangs must say so while the job can still report it, rather than
# run to the job's own cap and leave nothing but a cancelled leg.
STEP_TIMEOUT=2400

work=${HURD_WORK:-${RUNNER_TEMP:-/var/tmp}/hurd}
cache=${HURD_CACHE:-$work/cache}
srcdir=${GITHUB_WORKSPACE:-$(cd "$(dirname "$0")/.." && pwd)}
img=$work/hurd.img
key=$work/id_vm
mnt=$work/mnt
qemu_pid=
loop=

fail() {
    echo "hurd-vm: $*" >&2
    exit 1
}

cleanup() {
    set +e
    test -z "$qemu_pid" || kill "$qemu_pid" 2>/dev/null
    # Before the detach: a failure between the mount and the umount below
    # leaves the filesystem mounted, and losetup then refuses with EBUSY.
    mountpoint -q "$mnt" && sudo umount "$mnt"
    test -z "$loop" || sudo losetup -d "$loop"
}
trap 'set +e; cleanup' EXIT

# Provisioning runs as root, the build and the suite do not. A second ssh login
# rather than su, which wants a terminal the ssh channel does not give it.
as_user() {
    timeout "$STEP_TIMEOUT" ssh -p "$SSH_PORT" -i "$key" \
        -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
        -o LogLevel=ERROR -o ConnectTimeout=10 \
        "$VM_USER@127.0.0.1" "$@"
}

ssh_vm() {
    timeout "$STEP_TIMEOUT" ssh -p "$SSH_PORT" -i "$key" \
        -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
        -o LogLevel=ERROR -o ConnectTimeout=10 \
        root@127.0.0.1 "$@"
}

# The image runs on the VGA console, so the serial log is usually empty and a
# picture of the screen is the only evidence of where a wedged VM stopped.
screendump() {
    printf 'screendump %s\n' "$work/screen.ppm" |
        timeout 30 socat - "unix-connect:$work/monitor" >/dev/null 2>&1 || true
}

# The boot probe only. ConnectTimeout covers the TCP connect, and qemu's slirp
# accepts that immediately, so a guest whose sshd never sends a banner would
# block here until the job's own cap.
ssh_probe() {
    timeout 30 ssh -p "$SSH_PORT" -i "$key" \
        -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
        -o LogLevel=ERROR -o ConnectTimeout=10 \
        root@127.0.0.1 true
}

mkdir -p "$work" "$cache"

echo "::group::Fetch the Hurd image"
if ! test -r "$cache/$IMG_NAME"; then
    curl -fsSL -o "$cache/$IMG_NAME.part" "$IMG_BASE/$IMG_NAME"
    mv "$cache/$IMG_NAME.part" "$cache/$IMG_NAME"
fi
echo "$IMG_SHA512  $cache/$IMG_NAME" | sha512sum -c -
rm -f "$img"
# Extracted, not piped: the tarball stores the holes and tar only recreates them
# when it writes a file, so -O would cost the full ten gigabytes after the
# resize below.
tar -xJf "$cache/$IMG_NAME" -C "$work"
extracted=$(echo "$work"/debian-hurd-*.img)
test -s "$extracted" || fail "no image came out of $IMG_NAME"
mv "$extracted" "$img"
echo "::endgroup::"

echo "::group::Install an ssh key"
# The image's README names partitions 2 and 5, which is the i386 layout. The
# amd64 image has two primaries and no extended partition, so ask the image
# rather than trust the document.
root_part=$(sfdisk -J "$img" | python3 -c '
import json, re, sys

parts = json.load(sys.stdin)["partitiontable"]["partitions"]
ext2 = [p for p in parts if str(p.get("type", "")).lower().removeprefix("0x") == "83"]
if not ext2:
    sys.exit("no type-83 partition")
last = max(ext2, key=lambda p: p["start"])
print(re.search(r"([0-9]+)$", last["node"]).group(1))
')
test -n "$root_part" || fail "no ext2 partition in $img"
echo "root filesystem is partition $root_part"

loop=$(sudo losetup --find --show --partscan "$img")
part=${loop}p${root_part}
# losetup returns before udev has made the partition node.
for _ in $(seq 50); do
    test -b "$part" && break
    sleep 0.2
done
test -b "$part" || fail "$part never appeared after losetup --partscan"

rm -f "$key" "$key.pub"
ssh-keygen -q -t ed25519 -N '' -C hurd-vm -f "$key"
mkdir -p "$mnt"
sudo mount "$part" "$mnt"
sudo mkdir -p "$mnt/root/.ssh"
sudo cp "$key.pub" "$mnt/root/.ssh/authorized_keys"
sudo chmod 700 "$mnt/root/.ssh"
sudo chmod 600 "$mnt/root/.ssh/authorized_keys"
# The same key for the unprivileged account the suite runs under, owned by it.
ids=$(sudo awk -F: -v u="$VM_USER" '$1 == u { print $3 ":" $4 }' "$mnt/etc/passwd")
test -n "$ids" || fail "the image has no $VM_USER account to build as"
sudo mkdir -p "$mnt$VM_HOME/.ssh"
sudo cp "$key.pub" "$mnt$VM_HOME/.ssh/authorized_keys"
sudo chmod 700 "$mnt$VM_HOME/.ssh"
sudo chmod 600 "$mnt$VM_HOME/.ssh/authorized_keys"
sudo chown -R "$ids" "$mnt$VM_HOME/.ssh"
# A drop-in rather than an append, because sshd keeps the FIRST value it reads
# and Debian's sshd_config includes this directory on its opening line.
sudo grep -q '^Include /etc/ssh/sshd_config.d/' "$mnt/etc/ssh/sshd_config" ||
    fail "this image's sshd_config includes no drop-in directory"
sudo mkdir -p "$mnt/etc/ssh/sshd_config.d"
printf 'PermitRootLogin prohibit-password\n' |
    sudo tee "$mnt/etc/ssh/sshd_config.d/00-hurd-vm.conf" >/dev/null
sudo umount "$mnt"
sudo losetup -d "$loop"
loop=
echo "::endgroup::"

echo "::group::Boot the VM"
qemu-system-x86_64 -enable-kvm -m "$VM_MEM" -smp "$VM_CPUS" \
    -drive "file=$img,format=raw,cache=writeback" \
    -netdev "user,id=n0,hostfwd=tcp:127.0.0.1:$SSH_PORT-:22" \
    -device e1000,netdev=n0 \
    -display none -serial "file:$work/console.log" \
    -monitor "unix:$work/monitor,server,nowait" &
qemu_pid=$!

# Elapsed, not a count of sleeps: a probe that hangs to its own timeout would
# otherwise let the loop run several times the budget.
start=$SECONDS
until ssh_probe 2>/dev/null; do
    kill -0 "$qemu_pid" 2>/dev/null || fail "qemu exited during boot"
    waited=$((SECONDS - start))
    test "$waited" -lt "$BOOT_TIMEOUT" || {
        screendump
        fail "no ssh after ${BOOT_TIMEOUT}s; console tail:
$(tail -40 "$work/console.log" 2>/dev/null)"
    }
    sleep 5
done
echo "ssh answered after $((SECONDS - start))s"
# Otherwise the whole verdict is credited to Hurd on the word of whatever
# answered port 2222.
kernel=$(ssh_vm uname -s)
test "$kernel" = GNU || fail "the VM reports uname -s = $kernel, not GNU"
ssh_vm uname -a
echo "::endgroup::"

echo "::group::Install the build dependencies"
ssh_vm 'set -eu
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends \
        build-essential autoconf automake libtool autoconf-archive \
        zlib1g-dev libssl-dev python3 procps'
echo "::endgroup::"

# The buildds do not build as root, and the suite notices: httrack warns on
# every run, the file-size limit does not bind, and a test that needs a write
# to fail finds that none does.
as_user true || fail "cannot log in as $VM_USER"

echo "::group::Copy the tree in"
# A build that runs out of room fails somewhere in the middle and reads as a
# compile error, so say so here instead.
free=$(ssh_vm "df -k $VM_HOME | awk 'NR == 2 { print \$4 }'")
test "$free" -ge "$NEED_KB" ||
    fail "only ${free}K free on the VM, need ${NEED_KB}K"
# The tree is sent over ssh rather than cloned, because the VM has no route to
# an unpushed ref and a clone would fetch history nobody reads here.
tar -C "$srcdir" --exclude=.git -cf - . |
    ssh_vm "set -eu
        rm -rf $VM_HOME/httrack
        mkdir -p $VM_HOME/httrack
        tar -C $VM_HOME/httrack -xf -
        chown -R $VM_USER $VM_HOME/httrack"
echo "::endgroup::"

echo "::group::Build"
as_user "set -eu
    cd $VM_HOME/httrack
    ./bootstrap
    mkdir -p $VM_HOME/bld
    cd $VM_HOME/bld
    bash $VM_HOME/httrack/configure
    make -j2"
echo "::endgroup::"

echo "::group::Test"
rc=0
# -j2, not the harness default: the VM has one processor and 2G, and at -j4 it
# stopped answering ssh at a different test on each of two runs.
as_user "set -eu; cd $VM_HOME/bld && make check -j2" || rc=$?
test "$rc" -eq 0 || screendump
mkdir -p "$work/out"
ssh_vm "cat $VM_HOME/bld/tests/test-suite.log 2>/dev/null" >"$work/out/test-suite.log" || true
echo "::endgroup::"

test "$rc" -eq 0 || {
    echo "::error::the suite failed on GNU/Hurd (exit $rc)"
    tail -200 "$work/out/test-suite.log" || true
    exit "$rc"
}

# automake counts a SKIP as success, so a suite that skipped everything exits 0
# and would report a pass here. That is the vacuous result this whole job exists
# to avoid, and the buildds prove a Hurd runs nearly all of these tests.
banner=$(command grep -E '^# (TOTAL|PASS|SKIP|FAIL|ERROR):' "$work/out/test-suite.log") ||
    fail "no result banner in the suite log, so nothing proves the suite ran"
echo "$banner"
total=$(awk '/^# TOTAL:/ { print $3 }' <<<"$banner")
pass=$(awk '/^# PASS:/ { print $3 }' <<<"$banner")
if test -z "$total" || test -z "$pass"; then
    fail "could not read TOTAL and PASS out of the banner"
fi
test "$pass" -ge $((total / 2)) ||
    fail "only $pass of $total tests passed, so most of the suite never ran"

echo "hurd-vm: the suite passed on GNU/Hurd ($pass of $total)"
