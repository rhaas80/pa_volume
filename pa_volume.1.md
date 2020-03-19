% PA_VOLUME(1) 0.1 | Set remembered PulseAudio volume

## NAME
pa_volume - set remembered PulseAudio volume

## SYNOPSIS
pa_volume [OPTIONS] [CLIENT] [VOLUME] [SINK-NAME]

## DESCRIPTION
Get or set the remembered volume of CLIENT to VOLUME % and setting its target
sink to SINK-NAME.

If SINK-NAME is not given then the target sink is left unchanged.

If VOLUME is not given, the the current volume is reported.

If CLIENT is not given, volumes for all clients are reported.

Mandatory arguments to long options are mandatory for short options too.

**`-d`, `--show-device=SINK-NAME`**

: Show name of sink client outputs to

**`-s`, `--server=SERVER`**

: Use server SERVER instead of default daemon

**`-h`, `--help`**

: Show this help

### Exit status:
0
: if OK,

1
: if the specified client could not be found,

2
: if anything failed.

## EXAMPLES
Set volume of paplay to 66% on a PCI sound device

    pa_volume paplay 66 alsa_output.pci-0000_00_1f.3.analog-stereo

Set volume of paplay to 50.1%

    pa_volume paplay 50.

Show curernt volume of paplay

    pa_volume paplay

Show all client volumes
    
    pa_volume

## REPORTING BUGS
Please report any bugs found on
[GitHub](https://github.com/rhaas80/pa_volume).

## AUTHOR
Roland Haas, reachable at rhaas@ncsa.illinois.edu.

## COPYRIGHT
Copyright (C) 2017 The Board of Trustees of the University of Illinois

## SEE ALSO

pacmd(1), pavucontrol(1)
