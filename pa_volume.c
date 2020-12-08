#if 0
gcc -g3 -Wall pa_volume.c -lpulse
exit
#endif
/*
    pa_volume sets the client volume in PA's client database
    Copyright (C) 2017 - 2020 The Board of Trustees of the University of
    Illinois

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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>
#include <pulse/pulseaudio.h>
#include <pulse/ext-stream-restore.h>

static const char *client;
static char *server = NULL;
static double volume = -1.;
static bool relative_volume = false;
static bool toggle_mute = false;
static bool set_mute = false;
static bool set_nomute = false;
static const char *device = NULL;
static int show_device = 0;

static int client_found = 0;

// version string to match man page
#define VERSION "0.1.3"

// PA_CLAMP_VOLUME fails for me since PA_CLAMP_UNLIKELY is not defined
#define CLAMP_VOLUME(v) \
  ((v) < PA_VOLUME_MUTED ? PA_VOLUME_MUTED : \
   (v) > PA_VOLUME_MAX ? PA_VOLUME_MAX : (v))

// exit codes used by us
#define PAVO_EXIT_SUCCESS 0
#define PAVO_EXIT_CLIENT_NO_FOUND 1
#define PAVO_EXIT_FAILURE 2

// the actual worker that checks if we have found the client we are looking for
// then updates its volume
static void read_callback(pa_context *context,
        const pa_ext_stream_restore_info *info,
        int eol,
        void *userdata)
{
  assert(info || eol);

  if(info) {
    if(strstr(info->name, "sink-input-by-application-name:") ||
       strstr(info->name, "sink-input-by-media-role:")) {
      const int take_action = (volume >= 0. || relative_volume ||
                               toggle_mute || set_mute || set_nomute) &&
                              (client &&
                               strcmp(strchr(info->name, ':')+1, client) == 0);
      const int show_volume = !(volume >= 0. || relative_volume ||
                                toggle_mute || set_mute || set_nomute) && (
                              !client ||
                              strcmp(strchr(info->name, ':')+1, client) == 0);
      if(client && strcmp(strchr(info->name, ':')+1, client) == 0)
        client_found = 1;

      if(take_action) {
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
            pa_mainloop_quit((pa_mainloop*)userdata, PAVO_EXIT_FAILURE);
          }
        }
        if(new_info.volume.channels == 0) {
          new_info.volume.channels = 2;
        }
        if(volume >= 0. || relative_volume) {
          pa_volume_t channel_volume;
          if (relative_volume) {
            double curr_vol = pa_cvolume_avg(&info->volume) / (double)PA_VOLUME_NORM;
            channel_volume = CLAMP_VOLUME((curr_vol + volume) * PA_VOLUME_NORM);
          }
          else
            channel_volume = CLAMP_VOLUME((pa_volume_t)(volume*PA_VOLUME_NORM));
          pa_cvolume_set(&new_info.volume, new_info.volume.channels,
                         channel_volume);
        } else if(toggle_mute) {
          new_info.mute = !new_info.mute;
        } else if(set_mute) {
          new_info.mute = 1;
        } else if(set_nomute) {
          new_info.mute = 0;
        } else {
          assert(0 && "Internal error: unexpected action");
        }
        if(device) {
          new_info.device = device;
        }
        // use REPLACE rather than SET to keep the other client's information
        // intact
        pa_operation *write_op = pa_ext_stream_restore_write(
          context, PA_UPDATE_REPLACE, &new_info, 1, 1, NULL, NULL);
        if(write_op) {
          pa_operation_unref(write_op);
        } else {
          fprintf(stderr, "pa_ext_stream_restore_write() failed: %s\n",
                  pa_strerror(pa_context_errno(context)));
          pa_mainloop_quit((pa_mainloop*)userdata, PAVO_EXIT_FAILURE);
        }
      }
      if(show_volume) {
        char buf[PA_VOLUME_SNPRINT_MAX];
        if(info->channel_map.channels != 0) {
          pa_volume_snprint(buf, sizeof(buf), pa_cvolume_avg(&info->volume));
        } else {
          pa_volume_snprint(buf, sizeof(buf), PA_VOLUME_NORM);
        }
        if(show_device) {
          printf("client: %s %s [%s]%s\n", strchr(info->name, ':')+1, buf,
                 info->device ? info->device : "default",
                 info->mute ? " (muted)" : "");
        } else {
          printf("client: %s %s%s\n", strchr(info->name, ':')+1, buf,
                 info->mute ? " (muted)" : "");
        }
      }
    }
  }
  if(eol)
    pa_mainloop_quit((pa_mainloop*)userdata, PAVO_EXIT_SUCCESS);
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
      pa_mainloop_quit((pa_mainloop*)userdata, PAVO_EXIT_FAILURE);
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
  case PA_CONTEXT_TERMINATED:
  default:
    break;
  case PA_CONTEXT_FAILED:
    fprintf(stderr, "failed to connect: %s\n",
            pa_strerror(pa_context_errno(context)));
    pa_mainloop_quit((pa_mainloop*)userdata, PAVO_EXIT_FAILURE);
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
      pa_mainloop_quit((pa_mainloop*)userdata, PAVO_EXIT_FAILURE);
    }
    }
    break;
  }
}

static void usage(const char *argv0, const struct option *longopts,
                  const char *opthelp[], FILE *log)
{
  fprintf(log, "usage: %s [OPTIONS] [client] [volume|mute|unmute|toggle] [sink-name]\n", argv0);
  fprintf(log, "Get / set stored volume for a pulseaudio client.\n\n");
  fprintf(log, "sink-name is the name as output by: pacmd list-sinks\n\n");
  fprintf(log, "Examples:\n");
  fprintf(log, "  # set volume of paplay to 66%% on a PCI sound device\n");
  fprintf(log, "  %s paplay 66 alsa_output.pci-0000_00_1f.3.analog-stereo\n", argv0);
  fprintf(log, "  %s paplay 50.1   # set volume of paplay to 50.1%%\n", argv0);
  fprintf(log, "  %s paplay toggle # toggle mute status of paplay\n", argv0);
  fprintf(log, "  %s paplay        # show curernt volume of paplay\n", argv0);
  fprintf(log, "  %s               # show all client volumes\n\n", argv0);
  fprintf(log, "Options:\n\n");
  // computes maximum length of all option names to align output
  int maxlen = 0;
  for(const struct option *opt = longopts ; opt->name ; opt++) {
    int len = strlen(longopts->name);
    if(len > maxlen)
      maxlen = len;
  }

  // output table of options
  int buflen = 2*maxlen+4; // space for "--foo=FOO\0"
  char buf[buflen];
  for(const struct option *opt = longopts ; opt->name ; opt++) {
    // make a string "--foo=FOO" or "--foo" for a description
    const int starti = snprintf(buf, buflen, "--%s", opt->name);
    assert(starti < buflen);
    if(opt->has_arg) {
      assert(starti < buflen);
      buf[starti] = '=';
      const int len = strlen(opt->name);
      for(int i = 0 ; i < len ; i++) {
        assert(starti + 1 + i < buflen);
        buf[starti + 1 + i] = toupper(opt->name[i]);
      }
      assert(starti + 1 + len < buflen);
      buf[starti + 1 + len] = '\0';
    }
    int ind = opt - longopts;
    if(opt->val != 0) {
      fprintf(log, "-%c %-*s  %s\n", opt->val, buflen-1, buf, opthelp[ind]);
    } else {
      fprintf(log, "   %-*s  %s\n", buflen-1, buf, opthelp[ind]);
    }
  }
}

void version(void)
{
  printf("pa_volume %s\n", VERSION);
  printf("Copyright (C) 2017 - 2020 The Board of Trustees of the University of Illinois\n");
  printf("License GPLv2+: GNU GPL version 2 or later <https://gnu.org/licenses/gpl.html>.\n");
  printf("This is free software: you are free to change and redistribute it.\n");
  printf("There is NO WARRANTY, to the extent permitted by law.\n");
  printf("\n");
  printf("Written by Roland Haas.\n");
}

static void parse_args(int argc, char **argv)
{
  static const struct option longopts[] = {
    {"show-device", no_argument, NULL, 'd'},
    {"server", required_argument, NULL, 's'},
    {"help",   no_argument,       NULL, 'h'},
    {"version",   no_argument,    NULL, 0},
    {0,        0,                 0,     0 },
  };
  static const char *opthelp[] = {
    "Show name of sink client outputs to",
    "The name of the server to connect to",
    "Show this help",
    "Output version information and exit"
  };
  char optstring[2*sizeof(longopts)/sizeof(longopts[0])];
  for(int i = 0, j = 0 ; i < sizeof(longopts)/sizeof(longopts[0]) - 1 ; i++) {
    assert(j < sizeof(optstring));
    optstring[j++] = (char)longopts[i].val;
    if(longopts[i].has_arg) {
      assert(j < sizeof(optstring));
      optstring[j++] = ':';
    }
    // optstring always has space for the terminating NUL b/c of the empty
    // entry at the end
    assert(j < sizeof(optstring));
    optstring[j] = '\0';
  }

  int opt, longidx;
  while((opt = getopt_long(argc, argv, optstring, longopts, &longidx)) != -1) {
    switch(opt)
    {
      case 'd':
        show_device = 1;
        break;
      case 's':
        server = optarg;
        break;
      case 'h':
        usage(argv[0], longopts, opthelp, stdout);
        exit(PAVO_EXIT_SUCCESS);
        break;
      case 0: // long option only
        if(strcmp(longopts[longidx].name, "version") == 0) {
          version();
          exit(PAVO_EXIT_SUCCESS);
        } else {
          fprintf(stderr, "unexpected long-only option '%s'\n",
                  longopts[longidx].name);
          exit(PAVO_EXIT_FAILURE);
        }
        break;
      case '?':
        usage(argv[0], longopts, opthelp, stderr); // TODO: pass in argv[optind]?
        exit(PAVO_EXIT_FAILURE);
        break;
      default:
        fprintf(stderr, "getopt returned character cod 0x%x??\n", opt);
        exit(PAVO_EXIT_FAILURE);
        break;
    }
  }
  if(optind < argc)
    client = argv[optind++];
  if(optind < argc) {
    char *endptr = NULL;
    if(strcmp(argv[optind], "toggle") == 0) {
      toggle_mute = true;
    } else if(strcmp(argv[optind], "mute") == 0) {
      set_mute = true;
    } else if(strcmp(argv[optind], "unmute") == 0) {
      set_nomute = true;
    } else {
      if ('-' == argv[optind][0] || '+' == argv[optind][0]) {
        relative_volume = true;
      }
      volume = strtod(argv[optind], &endptr);
      if(*endptr != '\0') {
        if(endptr == argv[optind]) {
          fprintf(stderr, "Invalid argument '%s' could not be read a number\n",
                  endptr);
        } else {
          fprintf(stderr, "Extra characters '%s' after number '%.*s'\n", endptr,
                  (int)(endptr - argv[optind]), argv[optind]);
        }
        exit(PAVO_EXIT_FAILURE);
      }
      if(!relative_volume && (volume < 0. || volume > 100.)) {
        fprintf(stderr, "Invalid volume %g. Must be between 0 and 100.\n",
                volume);
        exit(PAVO_EXIT_FAILURE);
      }
      volume /= 100.;
    }
    if (!relative_volume)
      assert(toggle_mute + set_mute + set_nomute + (volume >= 0.) == 1);
    optind += 1;
  }
  if(optind < argc)
    device = argv[optind++];
  if(optind < argc) {
    while(optind < argc)
      fprintf(stderr, "extra argument '%s'\n", argv[optind++]);
    usage(argv[0], longopts, opthelp, stderr);
    exit(PAVO_EXIT_FAILURE);
  }
}

int main(int argc, char **argv)
{
  parse_args(argc, argv);

  // set up callbacks to first wait for pulseaudio to become ready, then check
  // for the presence of module-stream-restore then loop over all safed states,
  // then set new states with the new volume
  int retval;
  pa_mainloop *mainloop = pa_mainloop_new();
  if(mainloop) {
    pa_mainloop_api *mainloopapi = pa_mainloop_get_api(mainloop);
    if(mainloopapi) {
      pa_context *context = pa_context_new(mainloopapi, "Volume setter toy");
      if(context) {
        if(pa_context_connect(context, server, 0, NULL) >= 0) {
          // set up callback for pulseaudio to become ready and fire off the chain of
          // callbacks
          pa_context_set_state_callback(context, state_callback, mainloop);
          if(pa_mainloop_run(mainloop, &retval) < 0) {
            fprintf(stderr, "pa_mainloop_run() failed\n");
            retval = PAVO_EXIT_FAILURE;
          }

          // tear everything donw in reverse order
          pa_context_disconnect(context);
        } else {
          fprintf(stderr, "pa_context_connect() failed: %s\n",
                  pa_strerror(pa_context_errno(context)));
          retval = PAVO_EXIT_FAILURE;
        }
        pa_context_unref(context);
      } else {
        fprintf(stderr, "pa_context_new() failed\n");
        retval = PAVO_EXIT_FAILURE;
      }
    } else {
      fprintf(stderr, "pa_mainloop_get_api() failed\n");
      retval = PAVO_EXIT_FAILURE;
    }
    pa_mainloop_free(mainloop);
  } else {
    fprintf(stderr, "pa_mainloop_new() failed");
    retval = PAVO_EXIT_FAILURE;
  }

  if(retval != PAVO_EXIT_FAILURE) {
    if(client && !client_found) {
      fprintf(stderr, "Client '%s' not found.\n", client);
      retval = PAVO_EXIT_CLIENT_NO_FOUND;
    }
  }

  return retval;
}
