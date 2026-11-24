#!/bin/sh
# Build, test and package libfreenect in a Debian or Ubuntu container (CI).
# Runs as root inside the container, with the source tree as the working
# directory. RPI=1 adds Raspberry Pi's own repository first, which turns a
# Debian image into the package set of Raspberry Pi OS of the same release.
#
# A failure is reported as a GitHub annotation naming the step (and, for apt,
# the end of its output), so that it can be read without the job log.
set -eu
export DEBIAN_FRONTEND=noninteractive
SRC=$(pwd)
BUILD=/tmp/build
STEP=start

step() {
	STEP="$*"
	echo "== $*"
}

on_exit() {
	status=$?
	if [ "$status" -ne 0 ]; then
		echo "::error title=linux-distro.sh failed (exit $status)::in step: $STEP"
	fi
}
trap on_exit EXIT

# apt-get with its output kept, so that a failure can be annotated.
apt() {
	if ! apt-get "$@" > /tmp/apt.log 2>&1; then
		cat /tmp/apt.log
		echo "::error title=apt-get $1 failed::$(tail -n 6 /tmp/apt.log | tr '\n' ' ' | cut -c1-900)"
		exit 100
	fi
	cat /tmp/apt.log
}

step "base packages"
apt update
apt install -y --no-install-recommends ca-certificates curl

if [ "${RPI:-0}" = 1 ]; then
	. /etc/os-release
	step "Raspberry Pi repository for $VERSION_CODENAME"
	arch=$(dpkg --print-architecture)
	base=https://archive.raspberrypi.com/debian
	# Raspberry Pi's archive key comes from its keyring package (over HTTPS
	# from Raspberry Pi's archive): apt on trixie verifies with sqv, which
	# rejects the old raspberrypi.gpg.key.
	curl -fsSL "$base/dists/$VERSION_CODENAME/main/binary-$arch/Packages.gz" | gzip -dc > /tmp/rpi-Packages
	deb=$(awk '/^Package: raspberrypi-archive-keyring$/ { p = 1 } p && /^Filename: / { print $2; exit }' /tmp/rpi-Packages)
	if [ -z "$deb" ]; then
		echo "::error title=Raspberry Pi repository::no raspberrypi-archive-keyring in $base ($VERSION_CODENAME, $arch)"
		exit 1
	fi
	curl -fsSL "$base/$deb" -o /tmp/rpi-keyring.deb
	dpkg -i /tmp/rpi-keyring.deb
	key=/usr/share/keyrings/raspberrypi-archive-keyring.pgp
	[ -f "$key" ] || key=$(dpkg -L raspberrypi-archive-keyring | grep -E '^/usr/share/keyrings/.*\.(pgp|gpg)$' | head -n 1)
	echo "keyring: $key"
	echo "deb [signed-by=$key] $base/ $VERSION_CODENAME main" > /etc/apt/sources.list.d/raspi.list
	apt update
	apt -y full-upgrade
fi

step "build dependencies"
apt install -y --no-install-recommends \
	cmake build-essential pkg-config python3 file \
	libusb-1.0-0-dev libglfw3-dev libgl-dev libglu1-mesa-dev \
	xvfb xauth libgl1-mesa-dri udev

. /etc/os-release
echo "== $PRETTY_NAME ($(uname -m)), $(gcc --version | head -1), $(cmake --version | head -1)"
dpkg -s libusb-1.0-0-dev libglfw3-dev 2>/dev/null | grep -E '^(Package|Version):'

step "configure and build (-Werror)"
cmake -S "$SRC" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
	-DBUILD_EXAMPLES=ON -DBUILD_C_SYNC=ON -DBUILD_CPP=ON -DBUILD_FAKENECT=ON \
	-DFREENECT_GL_VIEWERS=GLFW -DBUILD_CPACK_DEB=ON -DFREENECT_WERROR=ON
if ! cmake --build "$BUILD" --parallel "$(nproc)" > /tmp/build.log 2>&1; then
	cat /tmp/build.log
	grep -E ': (error|warning): ' /tmp/build.log | head -n 20 | while IFS= read -r line; do
		echo "::error::$line"
	done
	exit 1
fi
tail -n 3 /tmp/build.log

step "unit tests (the window tests under Xvfb)"
xvfb-run -a ctest --test-dir "$BUILD" --output-on-failure

step "smoke tests (no Kinect attached)"
set +e
"$BUILD/bin/freenect-fwloader"; code=$?
set -e
test "$code" -eq 2 || { echo "freenect-fwloader should report no Kinect (exit 2), got $code"; exit 1; }
LIBFREENECT_HOTPLUG_POLL=1 "$BUILD/bin/freenect-hotplug" 2 --no-stream
for v in glview regview hiview chunkview micview glpclview cppview cpp_pcview v4l2; do
	test -x "$BUILD/bin/freenect-$v" || { echo "freenect-$v was not built"; exit 1; }
done

step "udev rules"
udevadm verify --no-style "$SRC/platform/linux/udev/51-kinect.rules"

step ".deb: build, install, remove"
(cd "$BUILD" && cpack -G DEB)
deb=$(ls "$BUILD"/*.deb)
dpkg-deb -c "$deb" | grep -E 'udev/rules.d/51-kinect.rules|systemd/system/freenect-fwloader.service|bin/freenect-fwloader$'
dpkg -i "$deb"
test -f /usr/lib/udev/rules.d/51-kinect.rules
test -f /usr/lib/systemd/system/freenect-fwloader.service
/usr/bin/freenect-fwloader --help > /dev/null
dpkg -r libfreenect-dev
test ! -e /usr/lib/udev/rules.d/51-kinect.rules
step "done: $PRETTY_NAME ($(uname -m))"
