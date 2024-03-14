# libnlctl

A FOSS replacement for nano leafs proprietary "PC Screen Mirror Lightstrip" software.

Platform Support: `Linux`.

## Architecture

- **`nlctld`** — system daemon that holds the HID device open and runs the current animation
- **`nlctl`** — CLI tool that sends commands to the daemon over a Unix socket
- **`libnlctl`** — the device + animation core (`include/nlctl.h`), linked into both binaries

The daemon is a single-threaded `epoll` loop: the listener socket, a `timerfd` animation
clock, a `timerfd` device-reconnect timer, and a `signalfd` for clean shutdown.

State is persisted to `/var/lib/nlctl/state.bin`, so the last-set mode is automatically
restored after a reboot. Reactive mode is session-only and is not persisted.

## Building

Requires a C99 compiler, `make`, and development headers for `hidapi` (hidraw backend),
`libX11`, and `libXext`.

```bash
# Arch
$ sudo pacman -S --needed base-devel hidapi libx11 libxext

$ make
```

Binaries land in `build/`. `make test` runs the unit and control-channel tests (no device
required). `make DEBUG=1` builds with ASan/UBSan.

## Installation

```bash
# Build
$ make

# Create the system user
$ sudo useradd -r -s /sbin/nologin -M nlctl

# Set up the udev rule
$ echo 'SUBSYSTEM=="hidraw", ATTRS{idVendor}=="37fa", ATTRS{idProduct}=="8202", GROUP="nlctl", MODE="0660"' \
    | sudo tee /etc/udev/rules.d/99-nanoleaf.rules
$ sudo udevadm control --reload && sudo udevadm trigger

# Install binaries to /usr/local/bin and the unit to /etc/systemd/system
$ sudo make install

# Enable the systemd service
$ sudo systemctl daemon-reload
$ sudo systemctl enable --now nlctld
```

The daemon starts immediately and will start automatically on every boot.

## Usage

```bash
$ nlctl rainbow
$ nlctl solid --color 255,0,128
$ nlctl breathing --color 0,100,255
$ nlctl wave --color 255,50,0
$ nlctl reactive              # syncs LEDs to screen content
$ nlctl off
$ nlctl status
```

`nlctl` and `nlctld` talk over `/run/nlctl/nlctl.sock` by default; set `NLCTL_SOCKET` on
both to use a different path (useful for running a user-session daemon without root).

### Reactive mode

Reactive mode captures the screen via X11 and sets each LED zone to the average color of the corresponding screen edge. 
`nlctl` forwards your `$DISPLAY` and `$XAUTHORITY` automatically through `nlctl reactive`. 
No extra configuration is needed as long as you run the command from within your desktop session.

No wayland support. Very doable, I just don't use it. 

#### Zone layout

The strip wraps around the monitor. Zone counts default to `10,10,10,10` (bottom, left, top, right) and can be tuned:

```bash
nlctl reactive --zones 12,8,12,8
```

## License

This project is available under the MIT license. See the LICENSE file for details.
