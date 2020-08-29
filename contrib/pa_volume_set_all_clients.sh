#!/bin/bash
# pa_volume_set_all_clients.sh
# set all known pulseaudio clients to one volume
# license CC0-1.0, warranty none

pa_volume_exe='./pa_volume'

volume="$1" # first argument passed to this script
if [[ -z "$volume" ]]
then
  echo "usage: $0 volume_in_percent"
  exit 1
fi

echo "setting all known pulseaudio clients to volume $volume"
while read line
do
  client="$(echo "$line" | sed -E 's/client: (.*[^ ]) +[0-9]+%/\1/')"
  echo "$volume $client"
  "$pa_volume_exe" "$client" "$volume"
done < <("$pa_volume_exe") # bash process substitution
