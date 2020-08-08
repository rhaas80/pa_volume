Pulseaudio volume setter
========================

pa_volume is a simple tool to set the remembered volume level of pulseaudio
clients. It requires module-stream-restore to be loaded (which is usually the
case) to function. When called without arguments it shows all the known clients
(running and non-running) and their remembered volume level. To set the volume
level pass it the name of the client followed by the volume in percent.

## Installation
No configure script is provided right now, but there is a simple Makefile. You
will need to have pkg-config and the development packages for libpulse installed.
On Debian/Ubuntu based distributions this is the pkg-config and libpulse-dev
packages.

```bash
sudo apt-get install pkg-config libpulse-dev
git clone https://github.com/rhaas80/pa_volume.git
cd pa_volume/
make
```

### AUR
This program is available on the AUR for Arch Linux-based distributions as
`pa_volume-git`.

## Examples

```bash
pa_volume paplay 50.1
```
will set the volume of paplay to 50.1%.

```bash
pa_volume
```
will list all known clients and their rememebered volume level for each
channel.
