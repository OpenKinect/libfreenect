# libfreenect on Linux

Tested targets: the newest Ubuntu release, the current and previous Ubuntu LTS,
and the newest Raspberry Pi OS (64-bit). Any distribution with libusb 1.0.26
or newer, udev and (optionally) systemd should work.

## Access without root

`udev/51-kinect.rules` gives access to every Kinect v1 USB function (camera
02ae/02bf, motor 02b0/02af, audio 02ad/02be and, with the firmware running,
02bb/02c3):

- the user logged in at the desktop gets access automatically (`uaccess`);
- members of the `plugdev` group get it too, which covers SSH sessions and
  headless machines. On Debian, Ubuntu and Raspberry Pi OS the first user is
  in `plugdev` already; otherwise run `sudo usermod -aG plugdev $USER` and log
  in again.

`cmake --install` puts the rules in place (see "Install locations"). To
install them by hand:

    sudo cp udev/51-kinect.rules /etc/udev/rules.d/
    sudo udevadm control --reload-rules
    sudo udevadm trigger --subsystem-match=usb --attr-match=idVendor=045e

then unplug the Kinect and plug it in again.

## Audio firmware at plug-in

The Kinect's audio controller starts in a bootloader and needs firmware
before its microphones (and, on the model 1473 and the Kinect for Windows, the
motor and LED) work. When systemd is present, the udev rules start
`freenect-fwloader.service` as soon as the bootloader appears; it uploads the
firmware and exits. The controller then re-enumerates as a standard USB audio
device, and `snd-usb-audio` offers its 4-channel microphone array to ALSA,
PipeWire and PulseAudio with no libfreenect application running:

    arecord -l                     # note the Kinect's card number, N
    arecord -D hw:N,0 -c 4 -f S32_LE -r 16000 -d 5 test.wav

The service looks for the firmware in `/usr/local/share/libfreenect` and
`/usr/share/libfreenect`. To install Microsoft's UAC firmware there (it is
downloaded from Microsoft, checked and extracted; nothing of Microsoft's is
shipped with libfreenect):

    sudo python3 fwfetcher.py --uac /usr/local/share/libfreenect

For a single user, `python3 fwfetcher.py --uac --user` installs it to
`$XDG_DATA_HOME/libfreenect` (`~/.local/share/libfreenect`) instead; then
applications upload it when they open the Kinect, but the plug-in service,
which runs as a system service, does not see it.

Only Microsoft's UAC firmware turns the microphones into a standard USB audio
device. If `audios.bin` (the Xbox firmware, libfreenect's own audio protocol)
is installed too, libfreenect prefers it on Linux; to make the plug-in
service use the UAC firmware anyway, run `sudo systemctl edit
freenect-fwloader.service` and add:

    [Service]
    Environment=LIBFREENECT_AUDIO_FIRMWARE=uac

To check the service: `journalctl -u freenect-fwloader.service`.

## The gspca_kinect kernel driver

Linux's `gspca_kinect` driver serves the Kinect camera as a V4L2 device.
libfreenect detaches it when it opens the camera and re-attaches it when it
closes the camera, so nothing needs to be configured. If you never want the
V4L2 device, `modprobe.d/kinect.conf` (installed to
`share/doc/libfreenect/linux`) has a commented-out blacklist line.

`snd-usb-audio` keeps the microphone array (interfaces 2 and 3 of the audio
controller); libfreenect never claims those.

## The Kinect as a camera for other programs (V4L2)

`freenect-v4l2` serves the Kinect's depth and colour streams as ordinary V4L2
cameras through [v4l2loopback](https://github.com/v4l2loopback/v4l2loopback),
so that OpenCV, GStreamer, ffmpeg, OBS, browsers and video-call programs can
use them. Unlike `gspca_kinect`, it gets everything libfreenect does: depth in
millimetres, hotplug, the model-specific fixes.

    sudo apt install v4l2loopback-dkms      # or linux-modules-extra-$(uname -r)
    sudo modprobe v4l2loopback devices=2 video_nr=10,11 \
         card_label="Kinect depth,Kinect video" exclusive_caps=1
    freenect-v4l2 --depth /dev/video10 --video /dev/video11

`exclusive_caps=1` makes each device look like a plain camera to its readers,
which browsers need. Then, for example:

    ffplay -f v4l2 /dev/video11                          # colour
    ffplay -f v4l2 /dev/video10                          # depth (16-bit)

| Option | Effect |
|---|---|
| `--depth DEV`, `--video DEV` | Where to write depth and colour (either or both) |
| `--depth-format y16` | Default. 16-bit grey whose value is the distance in millimetres (0 = no data), for programs that measure |
| `--depth-format yuyv` | A grey image (near = bright, 0.4–4.5 m), for programs that only show video |
| `--video-format yuyv` / `rgb24` | Colour as YUYV (default, accepted almost everywhere) or RGB24 |
| `--serial SERIAL` | Use this Kinect (default: the first one that arrives) |
| `--test-pattern` | Write moving test images without a Kinect, to check the loopback setup |

Both streams are 640×480 at 30 fps. It writes only to V4L2 *output* devices
(v4l2loopback) and refuses real cameras. When the Kinect is unplugged it waits
for it to come back; Ctrl+C stops it. To load v4l2loopback at boot, put
`v4l2loopback` in `/etc/modules-load.d/v4l2loopback.conf` and the options line
in `/etc/modprobe.d/v4l2loopback.conf`
(`options v4l2loopback devices=2 video_nr=10,11 card_label="Kinect depth,Kinect video" exclusive_caps=1`).

## One beamformed microphone (PipeWire)

With the UAC firmware running, PipeWire sees the Kinect's 4 microphones as one
4-channel input. libfreenect turns them into one microphone,
**Kinect beamformed microphone**, which every PipeWire, PulseAudio and JACK
application can pick. It follows the talker among 11 fixed beams from −50° to
+50°, as Microsoft's SDK does, and passes less of the room and of other
directions than any single microphone.

There are two ways, depending on the WirePlumber version (`wireplumber --version`):

**WirePlumber 0.5 or newer** (Ubuntu 25.04 and later, Debian 13, Raspberry Pi
OS trixie, Fedora). Nothing runs besides PipeWire. `cmake --install` puts in place:
- `lib/ladspa/freenect_beam.so`: libfreenect's beamformer as a LADSPA plugin;
- `share/libfreenect/pipewire/kinect-mic.conf`: a PipeWire filter graph with
  the plugin and a 90 Hz high-pass;
- `share/wireplumber/wireplumber.conf.d/51-freenect-kinect-mic.conf`: the
  WirePlumber rules.

When the Kinect's microphones appear, the rules open them as one raw 4-channel
input, load the filter graph on it and hide the raw input from applications,
as Asahi Linux does for the MacBook microphone arrays. Restart the session's
audio after installing: `systemctl --user restart wireplumber pipewire`. The
graph's controls are in `kinect-mic.conf`: `Mode` 0 follows the talker, 1
steers continuously, 2 keeps the beam at `Angle` (−50 to 50 degrees). `SDK
weights` 1 uses Microsoft's beam weights instead of libfreenect's (fetch them
with `fwfetcher.py --audio-weights`). `Noise suppression` and `AGC` work with
libfreenect built with SpeexDSP. To keep another microphone as the default,
lower `priority.session` there.

**WirePlumber 0.4** (Ubuntu 24.04). Use the `freenect-pipewire-mic` program
(built when the libpipewire-0.3 development files are installed). It does the
same as a small PipeWire client:

    systemctl --user enable --now freenect-pipewire-mic

Its options: `--beam auto|adaptive|ANGLE`, `--sdk-weights`,
`--noise-suppression`, `--agc`, `--target NODE` (another source than the
Kinect), `--name DESCRIPTION`, `--verbose` (reports the beam and the talker's
direction every 5 s). Don't use both ways at once: the WirePlumber rules hide
the raw input that the program looks for.

Either way, check with `wpctl status`: "Kinect beamformed microphone" is
listed under Sources. Angles are positive towards microphone 4, the outer
one of the three microphones on the same side of the Kinect.

## Install locations

| Install prefix | udev rules | systemd unit |
|---|---|---|
| `/usr` (distribution packages) | `/usr/lib/udev/rules.d` | `/usr/lib/systemd/system` |
| `/usr/local` (the default) | `/etc/udev/rules.d` | `/usr/local/lib/systemd/system` |
| anything else | `<prefix>/lib/udev/rules.d` | `<prefix>/lib/systemd/system` |

With another prefix, copy the files into place by hand. The CMake cache
variables `LIBFREENECT_UDEV_RULES_DIR` and `LIBFREENECT_SYSTEMD_UNIT_DIR`
override these. After installing, run `sudo udevadm control --reload-rules`
and `sudo systemctl daemon-reload`.
