#!/bin/bash
# End-to-end test of the beamformed microphone, with no Kinect: a private
# PipeWire and WirePlumber, a fake "Kinect" source playing a 4-channel
# recording of a talker 30 degrees off to one side (towards microphone 1),
# and recordings from the beamformed microphone with the beam fixed towards
# the talker and away from it. Passes when the beam towards the talker is
# clearly louder.
#
#   test-pipewire-mic.sh BUILD_DIR [filter|program|auto]
#
# filter:  WirePlumber 0.5's rule loads kinect-mic.conf with the LADSPA plugin
# program: freenect-pipewire-mic
# auto:    both if WirePlumber is 0.5 or newer, else the program (default)
#
# PW_ROOT=DIR uses PipeWire and WirePlumber unpacked under DIR (DIR/usr/...)
# instead of the installed ones. Exit 0: passed, 1: failed, 77: skipped.

set -u
B=$(cd "${1:?usage: $0 BUILD_DIR [filter|program|auto]}" && pwd)
MODE=${2:-auto}
T=$(mktemp -d /tmp/freenect-pw-test.XXXXXX)
trap 'pkill -P $$ 2>/dev/null; kill $(jobs -p) 2>/dev/null; rm -rf "$T"' EXIT

if [ -n "${PW_ROOT:-}" ]; then
	L=$PW_ROOT/usr/lib/$(gcc -dumpmachine 2>/dev/null || echo x86_64-linux-gnu)
	export PATH=$PW_ROOT/usr/bin:$PATH LD_LIBRARY_PATH=$L:${LD_LIBRARY_PATH:-}
	export PIPEWIRE_MODULE_DIR=$L/pipewire-0.3 SPA_PLUGIN_DIR=$L/spa-0.2
	export WIREPLUMBER_MODULE_DIR=$L/wireplumber-0.5 WIREPLUMBER_DATA_DIR=$PW_ROOT/usr/share/wireplumber
	PW_SHARE=$PW_ROOT/usr/share
else
	PW_SHARE=/usr/share
fi
for p in pipewire wireplumber pw-cat python3; do
	command -v $p > /dev/null || { echo "SKIP: $p is not installed"; exit 77; }
done
WPV=$(wireplumber --version 2>/dev/null | grep -oE '[0-9]+\.[0-9]+' | head -1)
WP05=0
case "$WPV" in 0.[0-4]) ;; "") ;; *) WP05=1 ;; esac
if [ "$MODE" = auto ]; then
	MODE=program
	[ "$WP05" = 1 ] && MODE="filter program"
fi
echo "WirePlumber $WPV; testing: $MODE"

mkdir -p "$T/run" "$T/cfg" "$T/wpconf/wireplumber.conf.d"
chmod 700 "$T/run"
cp -r "$PW_SHARE/pipewire" "$T/pwconf"
mkdir -p "$T/pwconf/pipewire.conf.d"
echo 'context.properties = { default.clock.rate = 48000 }' > "$T/pwconf/pipewire.conf.d/90-test.conf"
export XDG_RUNTIME_DIR=$T/run XDG_CONFIG_HOME=$T/cfg XDG_STATE_HOME=$T/state PIPEWIRE_CONFIG_DIR=$T/pwconf
export WIREPLUMBER_CONFIG_DIR=$T/wpconf:$PW_SHARE/wireplumber
# Ubuntu's WirePlumber requires a snapd module that a test system may lack
[ "$WP05" = 1 ] && echo 'wireplumber.profiles = { main = { support.snapd-permission-prompting = disabled } }' \
	> "$T/wpconf/wireplumber.conf.d/10-test.conf"

# the talker: band-limited noise, 30 degrees towards microphone 1
python3 -I - "$T/talker.wav" << 'PY'
import math, random, struct, sys
fs, n, xs, c = 16000, 16000 * 8, [-0.113, 0.036, 0.076, 0.113], 343.0
random.seed(1)
src, lp, hp, prev = [], 0.0, 0.0, 0.0
a_lp, a_hp = math.exp(-2 * math.pi * 3000 / fs), math.exp(-2 * math.pi * 300 / fs)
for i in range(n + 64):
    lp = (1 - a_lp) * random.gauss(0, 1) + a_lp * lp
    hp = a_hp * (hp + lp - prev); prev = lp
    src.append(hp)
peak = max(abs(v) for v in src); src = [v / peak * 0.5 for v in src]
out = bytearray()
for i in range(n):
    row = []
    for x in xs:
        t = i + 32 - x * math.sin(math.radians(30)) / c * fs
        k = int(math.floor(t)); f = t - k
        row.append(int((src[k] * (1 - f) + src[k + 1] * f) * 2**31 * 0.999))
    out += struct.pack('<4i', *row)
open(sys.argv[1], 'wb').write(b'RIFF' + struct.pack('<I', 36 + len(out)) + b'WAVEfmt ' +
    struct.pack('<IHHIIHH', 16, 1, 4, fs, fs * 16, 16, 32) + b'data' + struct.pack('<I', len(out)) + out)
PY

rms() { # the level of the last 2 s of a recording
	python3 -I - "$1" << 'PY'
import math, struct, sys
d = open(sys.argv[1], 'rb').read(); i = d.find(b'data'); s = d[i + 8:]
x = struct.unpack('<%dh' % (len(s) // 2), s[:len(s) // 2 * 2])[-32000:]
print('%.4f' % (math.sqrt(sum(v * v for v in x) / len(x)) / 32768 if x else 0))
PY
}

# run one case: $1 = filter|program, $2 = beam angle; prints the level
run_case() {
	local kind=$1 angle=$2 node target
	pkill -x wireplumber 2>/dev/null; pkill -x pipewire 2>/dev/null; sleep 0.3
	rm -f "$T/wpconf/wireplumber.conf.d/51-freenect-kinect-mic.conf"
	if [ "$kind" = filter ]; then
		sed "s#plugin = \".*\"#plugin = \"$B/lib/freenect_beam.so\"#; s#\"Mode\" = 0#\"Mode\" = 2#; s#\"Angle\" = 0#\"Angle\" = $angle#" \
			"$B/platform/linux/pipewire/kinect-mic.conf" > "$T/kinect-mic.conf"
		sed "s#filter-path = \".*\"#filter-path = \"$T/kinect-mic.conf\"#" \
			"$B/platform/linux/pipewire/51-freenect-kinect-mic.conf" > "$T/wpconf/wireplumber.conf.d/51-freenect-kinect-mic.conf"
		node=alsa_input.freenect.kinect-mics
		target=freenect.kinect-mic
	else
		node=alsa_input.usb-Microsoft_Xbox_NUI_Sensor_TEST-00.pro-input-0
		target=freenect_beam_mic
	fi
	pipewire > "$T/pw-$kind$angle.log" 2>&1 &
	sleep 1
	wireplumber > "$T/wp-$kind$angle.log" 2>&1 &
	sleep 2
	pw-cat --playback --rate 16000 --channels 4 --format s32 --channel-map AUX0,AUX1,AUX2,AUX3 \
		-P "{ media.class = Audio/Source node.name = $node node.description = \"fake Kinect\" }" "$T/talker.wav" \
		> "$T/play.log" 2>&1 &
	if [ "$kind" = program ]; then
		LD_LIBRARY_PATH=$B/lib:${LD_LIBRARY_PATH:-} "$B/bin/freenect-pipewire-mic" --verbose --beam "$angle" \
			> "$T/prog-$angle.log" 2>&1 &
	fi
	sleep 1.5
	pw-cat --record --rate 16000 --channels 1 --format s16 --target $target -P '{ node.name = freenect-test-rec }' \
		"$T/rec.wav" > "$T/rec.log" 2>&1 &
	sleep 4.5
	pkill -INT -f "pw-cat --record" ; sleep 0.5
	pkill -f "pw-cat --playback"; pkill -f freenect-pipewire-mic; pkill -x wireplumber; pkill -x pipewire
	sleep 0.3
	[ -s "$T/rec.wav" ] && rms "$T/rec.wav" || echo 0
}

fail=0
for kind in $MODE; do
	towards=$(run_case $kind -30)
	away=$(run_case $kind 30)
	ok=$(python3 -I -c "print(1 if $towards > 0.05 and $towards > 2 * $away else 0)")
	echo "$kind: level with the beam towards the talker $towards, away from it $away"
	if [ "$ok" = 1 ]; then
		echo "PASS: $kind"
	else
		echo "FAIL: $kind"
		fail=1
		tail -5 "$T"/wp-$kind*.log "$T"/prog-*.log 2> /dev/null
	fi
	if [ "$kind" = program ]; then
		grep " deg" "$T/prog--30.log" | tail -1
	fi
done
exit $fail
