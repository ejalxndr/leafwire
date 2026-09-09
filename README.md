# 🌿 leafwire 

An x86_64 CISC GNU/Linux driver for Nanoleaf PC Screen Mirror

## Dependencies[^1]

* c99 compiler
* make
* hidapi
* libX11
* libXext

On Arch Linux

```bash
$ sudo pacman -S --needed base-devel hidapi libx11 libxext
```

## Installation[^2]

Build leafwire core
```bash
$ make
```
Create the system user
```
$ sudo useradd -r -s /sbin/nologin -M leafwire
```
Set up the udev rule
```
$ echo 'SUBSYSTEM=="hidraw",'\
' ATTRS{idVendor}=="37fa",'\
' ATTRS{idProduct}=="8202",'\
' GROUP="leafwire",'\
' MODE="0660"' \
  | sudo tee /etc/udev/rules.d/99-nanoleaf.rules

$ sudo udevadm control --reload && sudo udevadm trigger
```
Install binaries to `/usr/local/bin` and the unit to `/etc/systemd/system`
```
$ sudo make install
```
Enable the systemd service
```
$ sudo systemctl daemon-reload
$ sudo systemctl enable --now lwd
```

The daemon starts immediately and will start automatically on every boot.

## Usage[^3]

```bash
$ lwctl rainbow
$ lwctl solid --color 255,0,128
$ lwctl breathing --color 0,100,255
$ lwctl wave --color 255,50,0
$ lwctl reactive              # syncs LEDs to screen content
$ lwctl off
$ lwctl status
```

`lwctl` and `lwd` talk over `/run/leafwire/leafwire.sock` by default. Set `LEAFWIRE_SOCKET` on both to use a different path.

### Zone layout

The strip wraps around the monitor. Zone counts default to `10,10,10,10` (bottom, left, top, right) and can be tuned:

```bash
lwctl reactive --zones 12,8,12,8
```

## License[^4]

This project is available under the MIT license. See the [LICENSE](./LICENSE.md). 

[^1]: Leafwire dependencies
[^2]: Leafwire installation
[^3]: Using leafwire
[^4]: Licensing options
