#!/bin/bash
#
# Build and run the suite on GNU/Hurd in a qemu VM, which no runner can do
# natively. Driven by .github/workflows/hurd-suite.yml.
#
# Four package builds broke on the hurd buildds and every one of them failed a
# TEST rather than the compile (#1668, #1717, #1719, #1775), so a cross-compile
# leg cannot see this class and the suite has to actually run.

set -euo pipefail

# Pinned rather than the undated debian-hurd.img.tar.xz alias, so the image
# cannot move under a cached key. Checksum from the archive's own SHA512SUMS.
IMG_BASE=https://cdimage.debian.org/cdimage/ports/latest/hurd-amd64
IMG_NAME=debian-hurd-amd64-20260314.img.tar.xz
IMG_SHA512=0c6151e7a402c065ef337841a2586e5c6a9799955d4f25d454cb7c6e6d54e7e2d52768aea0874a794aa285089a085c3b99702de475bc2fc4f250a2ce5a9f6cf0

# The image ships about 1.5G free, which a build plus the suite's fixtures
# outgrow.
GROW_BY=8G
# Hurd's SMP is young and the image is the tested single-processor configuration.
VM_CPUS=1
VM_MEM=4G
SSH_PORT=2222
# Hurd boots slower than Linux and the runner is shared, so this is generous.
BOOT_TIMEOUT=600

work=${HURD_WORK:-${RUNNER_TEMP:-/var/tmp}/hurd}
cache=${HURD_CACHE:-$work/cache}
srcdir=${GITHUB_WORKSPACE:-$(cd "$(dirname "$0")/.." && pwd)}
img=$work/hurd.img
key=$work/id_vm
qemu_pid=
loop=

fail() {
    echo "hurd-vm: $*" >&2
    exit 1
}

cleanup() {
    set +e
    test -z "$qemu_pid" || kill "$qemu_pid" 2>/dev/null
    test -z "$loop" || sudo losetup -d "$loop"
}
trap 'set +e; cleanup' EXIT

ssh_vm() {
    ssh -p "$SSH_PORT" -i "$key" \
        -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
        -o LogLevel=ERROR -o ConnectTimeout=10 \
        root@127.0.0.1 "$@"
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

echo "::group::Grow the root filesystem and install an ssh key"
qemu-img resize -f raw "$img" "+$GROW_BY"
# Partition 5 is the root filesystem, inside extended partition 2. Both have to
# follow the disk out, and parted takes the pair on stdin.
parted -s "$img" resizepart 2 100% resizepart 5 100%
loop=$(sudo losetup --find --show --partscan "$img")
# e2fsck reports 1 and 2 for errors it fixed by itself, which is not a failure.
sudo e2fsck -fp "${loop}p5" || test $? -le 2
sudo resize2fs "${loop}p5"

rm -f "$key" "$key.pub"
ssh-keygen -q -t ed25519 -N '' -C hurd-vm -f "$key"
mnt=$work/mnt
mkdir -p "$mnt"
sudo mount "${loop}p5" "$mnt"
sudo mkdir -p "$mnt/root/.ssh"
sudo cp "$key.pub" "$mnt/root/.ssh/authorized_keys"
sudo chmod 700 "$mnt/root/.ssh"
sudo chmod 600 "$mnt/root/.ssh/authorized_keys"
# The image leaves root without a password, which sshd refuses to let in even
# with a key unless root login is spelled out.
printf 'PermitRootLogin prohibit-password\n' |
    sudo tee -a "$mnt/etc/ssh/sshd_config" >/dev/null
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

waited=0
until ssh_vm true 2>/dev/null; do
    kill -0 "$qemu_pid" 2>/dev/null || fail "qemu exited during boot"
    test "$waited" -lt "$BOOT_TIMEOUT" || {
        # The image boots on the VGA console, so the serial log is usually
        # empty and a screendump is the only picture of where it stopped.
        printf 'screendump %s\n' "$work/screen.ppm" |
            timeout 30 socat - "unix-connect:$work/monitor" >/dev/null 2>&1 || true
        fail "no ssh after ${BOOT_TIMEOUT}s; console tail:
$(tail -40 "$work/console.log" 2>/dev/null)"
    }
    sleep 5
    waited=$((waited + 5))
done
echo "ssh answered after ${waited}s"
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

echo "::group::Copy the tree in"
# git archive of the worktree plus the submodule, because the VM has no network
# route to a private ref and cloning would fetch history nobody reads here.
tar -C "$srcdir" --exclude=.git -cf - . |
    ssh_vm 'set -eu; rm -rf /root/httrack; mkdir -p /root/httrack; tar -C /root/httrack -xf -'
echo "::endgroup::"

echo "::group::Build"
ssh_vm 'set -eu
    cd /root/httrack
    ./bootstrap
    mkdir -p /root/bld
    cd /root/bld
    bash /root/httrack/configure
    make -j2'
echo "::endgroup::"

echo "::group::Test"
rc=0
ssh_vm 'set -eu; cd /root/bld && make check -j4' || rc=$?
mkdir -p "$work/out"
ssh_vm 'cat /root/bld/tests/test-suite.log 2>/dev/null' >"$work/out/test-suite.log" || true
test "$rc" -eq 0 || {
    echo "::endgroup::"
    echo "::error::the suite failed on GNU/Hurd (exit $rc)"
    tail -200 "$work/out/test-suite.log" || true
    exit "$rc"
}
echo "::endgroup::"

echo "hurd-vm: the suite passed on GNU/Hurd"
