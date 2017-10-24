Pulseaudio volume setter
========================

pa_volume is a simple tool to set the remembered volume level of pulseaudio
clients. It requires module-stream-restore to be loaded (which is usually the
case) to function. When called without arguments it shows all the known clients
(running and non-running) and their remembered volume level. To set the volume
level pass it the name of the client followed by the volume in pecent.

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

