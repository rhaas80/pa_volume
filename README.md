Pulseaudio volume setter
========================

pa_volume is a simple tool to set the remembered volume level of pulseaudio
clients. It requires module-stream-restore to be loaded (which is usually the
case) to function. When called without arguments it shows all the known clients
(running and non-running) and their remembered volume level. To set the volume
level pass it the name of the client followed by the volume in percent.

## Compiling
No configure script is provided right now, but there is a simple Makefile. You
will need to have pkg-config and the development packages for libpulse
installed.  To create a man page, pandoc is required. On Debian/Ubuntu based
distributions these are the pkg-config, libpulse-dev, pandoc packages.

```bash
sudo apt-get install pkg-config libpulse-dev pandoc
git clone https://github.com/rhaas80/pa_volume.git
cd pa_volume/
make
```

You can then test the executable using `./pa_volume`.

Copy the executable (`pa_volume`) and man page (`pa_volume.1`) to
a directory in your `$PATH` and `$MANPATH`, respectively.

### AUR
This program is available on the AUR for Arch Linux-based distributions as
`pa_volume-git`.

### Alpine Linux

To compile and install on alpine Linux or other for Docker images these
commands, suggested by @anvda may be used:

```bash
RUN set -ex && apk --no-cache add  build-base libpulse pulseaudio-dev make git
RUN cd /tmp && \
    git clone https://github.com/rhaas80/pa_volume.git && \
    cd pa_volume && \
    PKG_CONFIG_PATH=/usr/lib  make && \
    cp  pa_volume /usr/local/bin
```

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

```bash
pa_volume paplay 66 alsa_output.pci-0000_00_1f.3.analog-stereo
```
will set the volume of paplay to 66% on a PCI sound device

```bash
pa_volume paplay toggle
```
will toggle mute for paplay
