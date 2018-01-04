#if 0
gcc -g3 -Wall pa_volume.c -lpulse
exit
#endif
/*
    pa_volume sets the client volume in PA's client database
    Copyright (C) 2017 The Board of Trustees of the University of Illinois

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    Authors:
      Roland Haas
*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <pulse/ext-stream-restore.h>

static const char *client;
static double volume = -1.;

// PA_CLAMP_VOLUME fails for me since PA_CLAMP_UNLIKELY is not defined
#define CLAMP_VOLUME(v) \
  ((v) < PA_VOLUME_MUTED ? PA_VOLUME_MUTED : \
   (v) > PA_VOLUME_MAX ? PA_VOLUME_MAX : (v))

// the actual worker that checks if we have found the client we are looking for
// then updates its volume
static void read_callback(pa_context *context,
        const pa_ext_stream_restore_info *info,
        int eol,
        void *userdata)
{
  assert(info || eol);

  if(info) {
    if(strstr(info->name, "sink-input-by-application-name:")) {
      const int set_volume = volume >= 0. && (
                             client &&
                             strcmp(strchr(info->name, ':')+1, client) == 0);
      const int show_volume = volume < 0. && (
                              !client ||
                              strcmp(strchr(info->name, ':')+1, client) == 0);

      if(set_volume) {
        // we found the client we are looking for, now make a new info struct
        // replacing just the volume
        pa_ext_stream_restore_info new_info = *info;
        // it seems that as if the volume was never set then the database
        // contains invalid entries so I have to make a (hopefully sane)
        // default. Ideally I would like to query PulseAudio for "default"
        // values.
        if(new_info.channel_map.channels == 0) {
          if(!pa_channel_map_init_stereo(&new_info.channel_map)) {
            fprintf(stderr, "pa_channel_map_init_stereo() faiiled\n");
            pa_mainloop_quit((pa_mainloop*)userdata, EXIT_FAILURE);
          }
        }
        if(new_info.volume.channels == 0) {
          new_info.volume.channels = 2;
        }
        pa_volume_t channel_volume =
          CLAMP_VOLUME((pa_volume_t)(volume*PA_VOLUME_NORM));
        pa_cvolume_set(&new_info.volume, new_info.volume.channels,
                       channel_volume);
        // use REPLACE rather than SET to keep the other client's information
        // intact
        pa_operation *write_op = pa_ext_stream_restore_write(
          context, PA_UPDATE_REPLACE, &new_info, 1, 1, NULL, NULL);
        if(write_op) {
          pa_operation_unref(write_op);
        } else {
          fprintf(stderr, "pa_ext_stream_restore_write() failed: %s\n",
                  pa_strerror(pa_context_errno(context)));
          pa_mainloop_quit((pa_mainloop*)userdata, EXIT_FAILURE);
        }
      }
      if(show_volume) {
        // TODO: output only if requested
        char buf[PA_VOLUME_SNPRINT_MAX];
        pa_volume_snprint(buf, sizeof(buf), pa_cvolume_avg(&info->volume));
        printf("client: %s %s\n", strchr(info->name, ':')+1, buf);
      }
    }
  }
  if(eol)
    pa_mainloop_quit((pa_mainloop*)userdata, EXIT_SUCCESS);
}

// wait for module-stream-restore
static void test_callback(
        pa_context *context,
        uint32_t version,
        void *userdata)
{
  if(version > 0) { // is version 0 "not present"?
    // loops over all safed states
    pa_operation *operation =
      pa_ext_stream_restore_read(context, read_callback,
                                 (pa_mainloop*)userdata);
    if(operation) {
      pa_operation_unref(operation);
    } else {
      fprintf(stderr, "pa_ext_stream_restore_read() failed: %s\n",
              pa_strerror(pa_context_errno(context)));
      pa_mainloop_quit((pa_mainloop*)userdata, EXIT_FAILURE);
    }
  }
}

// just wait for pulseaudio to become available
static void state_callback(pa_context *context, void *userdata) {
  pa_context_state_t state = pa_context_get_state(context);
  switch  (state) {
  case PA_CONTEXT_UNCONNECTED:
  case PA_CONTEXT_CONNECTING:
  case PA_CONTEXT_AUTHORIZING:
  case PA_CONTEXT_SETTING_NAME:
  case PA_CONTEXT_FAILED:
  case PA_CONTEXT_TERMINATED:
  default:
    break;
  case PA_CONTEXT_READY:
    {
    // we need module-stream-restore so check (and wait) for it
    pa_operation *test_op = pa_ext_stream_restore_test(context, test_callback,
                                                       (pa_mainloop*)userdata);
    if(test_op) {
      pa_operation_unref(test_op);
    } else {
      fprintf(stderr, "pa_ext_stream_restore_test() failed: %s\n",
              pa_strerror(pa_context_errno(context)));
      pa_mainloop_quit((pa_mainloop*)userdata, EXIT_FAILURE);
    }
    }
    break;
  }
}

int main(int argc, char **argv)
{
  // very crude, just take two arguments client name and volume (float, 100
  // being full volume)
  if((argc == 2 && strcmp(argv[1], "--help") == 0) || argc > 3) {
    fprintf(stderr, "usage: %s client volume%%\n", argv[0]);
    exit(0); // TODO: add exit failure
  }
  if(argc >= 2)
    client = argv[1];
  if(argc >= 3)
    volume = atof(argv[2])/100.;

  // set up callbacks to first wait for pulseaudio to become ready, then check
  // for the presence of module-stream-restore then loop over all safed states,
  // then set new states with the new volume
  int retval = 0;
  pa_mainloop *mainloop = pa_mainloop_new();
  if(mainloop) {
    pa_mainloop_api *mainloopapi = pa_mainloop_get_api(mainloop);
    if(mainloopapi) {
      pa_context *context = pa_context_new(mainloopapi, "Volume setter toy");
      if(context) {
        if(pa_context_connect(context, NULL, 0, NULL) >= 0) {
          // set up callback for pulseaudio to become ready and fire off the chain of
          // callbacks
          pa_context_set_state_callback(context, state_callback, mainloop);
          retval = pa_mainloop_run(mainloop, NULL);

          // tear everything donw in reverse order
          pa_context_disconnect(context);
        } else {
          fprintf(stderr, "pa_context_connect() failed: %s\n",
                  pa_strerror(pa_context_errno(context)));
          retval = EXIT_FAILURE;
        }
        pa_context_unref(context);
      } else {
        fprintf(stderr, "pa_context_new() failed\n");
        retval = EXIT_FAILURE;
      }
    } else {
      fprintf(stderr, "pa_mainloop_get_api() failed\n");
      retval = EXIT_FAILURE;
    }
    pa_mainloop_free(mainloop);
  } else {
    fprintf(stderr, "pa_mainloop_new() failed");
    retval = EXIT_FAILURE;
  }

  return retval;
}
