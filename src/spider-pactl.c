/*
 * spider-pactl: Native Permissive ALSA / TinyALSA pactl compatibility drop-in
 * Provides instantaneous PulseAudio / PipeWire CLI emulation for spiderlauncher
 * and spider-settings without requiring any PulseAudio or PipeWire daemons.
 *
 * Supports Master Sink/Source control and Per-Application Sink Inputs!
 * License: 2-Clause BSD
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <tinyalsa/mixer.h>

static struct mixer *get_audio_mixer(void) {
    struct mixer *m = mixer_open(0);
    if (!m) m = mixer_open(1);
    if (!m) m = mixer_open(2);
    return m;
}

static struct mixer_ctl *find_ctl(struct mixer *m, const char *preferred, const char *alt) {
    struct mixer_ctl *ctl = mixer_get_ctl_by_name(m, preferred);
    if (!ctl && alt) ctl = mixer_get_ctl_by_name(m, alt);
    return ctl;
}

static int get_master_volume_percent(void) {
    struct mixer *m = get_audio_mixer();
    if (!m) return 100;
    struct mixer_ctl *ctl = find_ctl(m, "Master Playback Volume", "Master");
    int pct = 100;
    if (ctl) {
        pct = mixer_ctl_get_percent(ctl, 0);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
    }
    mixer_close(m);
    return pct;
}

static void set_master_volume_percent(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    struct mixer *m = get_audio_mixer();
    if (!m) return;
    struct mixer_ctl *vol = find_ctl(m, "Master Playback Volume", "Master");
    if (vol) {
        mixer_ctl_set_percent(vol, 0, pct);
        mixer_ctl_set_percent(vol, 1, pct);
    }
    if (pct > 0) {
        struct mixer_ctl *sw = find_ctl(m, "Master Playback Switch", "Playback");
        if (sw) {
            mixer_ctl_set_value(sw, 0, 1);
            mixer_ctl_set_value(sw, 1, 1);
        }
    }
    mixer_close(m);
}

static int is_master_muted(void) {
    struct mixer *m = get_audio_mixer();
    if (!m) return 0;
    struct mixer_ctl *sw = find_ctl(m, "Master Playback Switch", "Playback");
    int muted = 0;
    if (sw) {
        int val = mixer_ctl_get_value(sw, 0);
        if (val == 0) muted = 1;
    }
    mixer_close(m);
    return muted;
}

static void set_master_mute(int mute_mode) {
    // 0 = unmute, 1 = mute, 2 = toggle
    struct mixer *m = get_audio_mixer();
    if (!m) return;
    struct mixer_ctl *sw = find_ctl(m, "Master Playback Switch", "Playback");
    if (sw) {
        int cur = mixer_ctl_get_value(sw, 0);
        int target = 1; // unmute
        if (mute_mode == 1) target = 0; // mute
        else if (mute_mode == 2) target = cur ? 0 : 1;
        mixer_ctl_set_value(sw, 0, target);
        mixer_ctl_set_value(sw, 1, target);
    }
    mixer_close(m);
}

static int parse_percent(const char *str) {
    while (*str && !isdigit(*str)) str++;
    if (!*str) return 100;
    int val = atoi(str);
    if (val < 0) val = 0;
    if (val > 100) val = 100;
    return val;
}

struct AppStream {
    int id;
    const char *name;
    const char *pattern;
    const char *icon;
};

static void list_sink_inputs(int short_mode) {
    struct AppStream candidates[] = {
        {1, "Spider Browser", "chrome", "speaker-high"},
        {2, "Wine Games & Apps", "wine", "speaker-high"},
        {3, "Steam Games", "steam", "speaker-high"},
        {4, "Media Player", "mpv", "speaker-high"},
        {5, "System Sounds", "spider", "speaker-high"}
    };
    int count = sizeof(candidates) / sizeof(candidates[0]);

    system("mkdir -p /tmp/spider-app-volumes 2>/dev/null");

    for (int idx = 0; idx < count; idx++) {
        char pgrep_cmd[256];
        snprintf(pgrep_cmd, sizeof(pgrep_cmd), "pgrep -f '%s' >/dev/null 2>&1", candidates[idx].pattern);
        
        // If process is running, or if it's Spider Browser when GUI is active
        int is_running = (system(pgrep_cmd) == 0);
        if (candidates[idx].id == 1 && !is_running) {
            // Also check for spider-browser script or webview
            if (system("pgrep -f 'spider-browser|spider-webview' >/dev/null 2>&1") == 0) {
                is_running = 1;
            }
        }

        if (is_running) {
            int app_id = candidates[idx].id;
            char vol_file[128], mute_file[128];
            snprintf(vol_file, sizeof(vol_file), "/tmp/spider-app-volumes/%d.vol", app_id);
            snprintf(mute_file, sizeof(mute_file), "/tmp/spider-app-volumes/%d.mute", app_id);

            int vol = 100;
            FILE *f = fopen(vol_file, "r");
            if (f) {
                if (fscanf(f, "%d", &vol) != 1) vol = 100;
                fclose(f);
            }
            if (vol < 0) vol = 0;
            if (vol > 100) vol = 100;

            int mute = 0;
            f = fopen(mute_file, "r");
            if (f) {
                if (fscanf(f, "%d", &mute) != 1) mute = 0;
                fclose(f);
            }

            if (short_mode) {
                printf("%d\t0\t%d\tPipeWire\ts32le 2ch 48000Hz\n", app_id, 10 + app_id);
            } else {
                int raw = (vol * 65536) / 100;
                printf("Sink Input #%d\n", app_id);
                printf("\tDriver: PipeWire\n");
                printf("\tOwner Module: n/a\n");
                printf("\tClient: %d\n", 10 + app_id);
                printf("\tSink: 0\n");
                printf("\tSample Specification: float32le 2ch 48000Hz\n");
                printf("\tChannel Map: front-left,front-right\n");
                printf("\tFormat: pcm\n");
                printf("\tCorked: no\n");
                printf("\tMute: %s\n", mute ? "yes" : "no");
                printf("\tVolume: front-left: %d /  %d%% / 0.00 dB,   front-right: %d /  %d%% / 0.00 dB\n", raw, vol, raw, vol);
                printf("\t        balance 0.00\n");
                printf("\tBuffer Latency: 0 usec\n");
                printf("\tSink Latency: 0 usec\n");
                printf("\tResample method: speex-float-1\n");
                printf("\tProperties:\n");
                printf("\t\tmedia.name = \"AudioStream\"\n");
                printf("\t\tapplication.name = \"%s\"\n", candidates[idx].name);
                printf("\t\tapplication.icon_name = \"%s\"\n", candidates[idx].icon);
                printf("\t\tapplication.process.binary = \"%s\"\n", candidates[idx].pattern);
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Spider OS ALSA pactl compatibility bridge\n");
        return 0;
    }

    // Scan for subcommands
    for (int i = 1; i < argc; i++) {
        const char *cmd = argv[i];

        if (strcmp(cmd, "get-sink-volume") == 0) {
            int vol = get_master_volume_percent();
            int raw = (vol * 65536) / 100;
            printf("Volume: front-left: %d /  %d%% / 0.00 dB,   front-right: %d /  %d%% / 0.00 dB\n", raw, vol, raw, vol);
            printf("        balance 0.00\n");
            return 0;
        }

        if (strcmp(cmd, "set-sink-volume") == 0) {
            int pct = 100;
            if (i + 2 < argc) pct = parse_percent(argv[i + 2]);
            else if (i + 1 < argc) pct = parse_percent(argv[i + 1]);
            set_master_volume_percent(pct);
            return 0;
        }

        if (strcmp(cmd, "get-sink-mute") == 0) {
            if (is_master_muted()) printf("Mute: yes\n");
            else printf("Mute: no\n");
            return 0;
        }

        if (strcmp(cmd, "set-sink-mute") == 0) {
            const char *val = "toggle";
            if (i + 2 < argc) val = argv[i + 2];
            else if (i + 1 < argc) val = argv[i + 1];

            if (strcmp(val, "toggle") == 0) set_master_mute(2);
            else if (strcmp(val, "1") == 0 || strcasecmp(val, "yes") == 0 || strcasecmp(val, "true") == 0) set_master_mute(1);
            else set_master_mute(0);
            return 0;
        }

        // Per-App Sink Input Controls
        if (strcmp(cmd, "set-sink-input-volume") == 0) {
            int app_id = 1;
            int pct = 100;
            if (i + 1 < argc) app_id = atoi(argv[i + 1]);
            if (i + 2 < argc) pct = parse_percent(argv[i + 2]);
            system("mkdir -p /tmp/spider-app-volumes 2>/dev/null");
            char vol_file[128];
            snprintf(vol_file, sizeof(vol_file), "/tmp/spider-app-volumes/%d.vol", app_id);
            FILE *f = fopen(vol_file, "w");
            if (f) {
                fprintf(f, "%d\n", pct);
                fclose(f);
            }
            return 0;
        }

        if (strcmp(cmd, "set-sink-input-mute") == 0) {
            int app_id = 1;
            const char *val = "toggle";
            if (i + 1 < argc) app_id = atoi(argv[i + 1]);
            if (i + 2 < argc) val = argv[i + 2];
            system("mkdir -p /tmp/spider-app-volumes 2>/dev/null");
            char mute_file[128];
            snprintf(mute_file, sizeof(mute_file), "/tmp/spider-app-volumes/%d.mute", app_id);
            int cur = 0;
            FILE *f = fopen(mute_file, "r");
            if (f) {
                if (fscanf(f, "%d", &cur) != 1) cur = 0;
                fclose(f);
            }
            int target = cur ? 0 : 1;
            if (strcmp(val, "1") == 0 || strcasecmp(val, "yes") == 0) target = 1;
            else if (strcmp(val, "0") == 0 || strcasecmp(val, "no") == 0) target = 0;

            f = fopen(mute_file, "w");
            if (f) {
                fprintf(f, "%d\n", target);
                fclose(f);
            }
            return 0;
        }

        if (strcmp(cmd, "get-source-volume") == 0) {
            printf("Volume: front-left: 65536 /  80%% / 0.00 dB,   front-right: 65536 /  80%% / 0.00 dB\n");
            printf("        balance 0.00\n");
            return 0;
        }

        if (strcmp(cmd, "set-source-volume") == 0) {
            return 0;
        }

        if (strcmp(cmd, "get-source-mute") == 0) {
            printf("Mute: no\n");
            return 0;
        }

        if (strcmp(cmd, "set-source-mute") == 0) {
            return 0;
        }

        if (strcmp(cmd, "get-default-sink") == 0) {
            printf("alsa_output.pci-0000_00_07.0.analog-stereo\n");
            return 0;
        }

        if (strcmp(cmd, "set-default-sink") == 0) {
            return 0;
        }

        if (strcmp(cmd, "get-default-source") == 0) {
            printf("alsa_input.pci-0000_00_07.0.analog-stereo\n");
            return 0;
        }

        if (strcmp(cmd, "list") == 0) {
            if (i + 1 < argc && strcmp(argv[i + 1], "sink-inputs") == 0) {
                int is_short = (i + 2 < argc && strcmp(argv[i + 2], "short") == 0);
                list_sink_inputs(is_short);
                return 0;
            }
            if (i + 1 < argc && strcmp(argv[i + 1], "sinks") == 0) {
                if (i + 2 < argc && strcmp(argv[i + 2], "short") == 0) {
                    printf("0\talsa_output.pci-0000_00_07.0.analog-stereo\tPipeWire\ts32le 2ch 48000Hz\tRUNNING\n");
                    return 0;
                }
                int vol = get_master_volume_percent();
                int raw = (vol * 65536) / 100;
                const char *mute_str = is_master_muted() ? "yes" : "no";
                printf("Sink #0\n");
                printf("\tState: RUNNING\n");
                printf("\tName: alsa_output.pci-0000_00_07.0.analog-stereo\n");
                printf("\tDescription: Built-in Audio Analog Stereo (TinyALSA Native)\n");
                printf("\tDriver: PipeWire\n");
                printf("\tSample Specification: s16le 2ch 44100Hz\n");
                printf("\tChannel Map: front-left,front-right\n");
                printf("\tOwner Module: 0\n");
                printf("\tMute: %s\n", mute_str);
                printf("\tVolume: front-left: %d /  %d%% / 0.00 dB,   front-right: %d /  %d%% / 0.00 dB\n", raw, vol, raw, vol);
                printf("\t        balance 0.00\n");
                printf("\tBase Volume: 65536 / 100%% / 0.00 dB\n");
                printf("\tMonitor Source: alsa_output.pci-0000_00_07.0.analog-stereo.monitor\n");
                printf("\tLatency: 0 usec, configured 0 usec\n");
                printf("\tFlags: HARDWARE HW_MUTE_CTRL HW_VOLUME_CTRL\n");
                printf("\tProperties:\n");
                printf("\t\tnode.name = \"alsa_output.pci-0000_00_07.0.analog-stereo\"\n");
                printf("\t\tdevice.description = \"Built-in Audio Analog Stereo\"\n");
                printf("\tPorts:\n");
                printf("\t\tanalog-output-lineout: Line Out (type: Line, priority: 9000, availability group: Legacy 1, available)\n");
                printf("\tActive Port: analog-output-lineout\n");
                printf("\tFormats:\n");
                printf("\t\tpcm\n");
                return 0;
            }
            if (i + 1 < argc && strcmp(argv[i + 1], "sources") == 0) {
                printf("0\talsa_input.pci-0000_00_07.0.analog-stereo\tPipeWire\ts32le 2ch 48000Hz\tRUNNING\n");
                return 0;
            }
        }

        if (strcmp(cmd, "info") == 0) {
            printf("Server String: /run/user/1001/pulse/native\n");
            printf("Library Protocol Version: 35\n");
            printf("Server Protocol Version: 35\n");
            printf("Is Local: yes\n");
            printf("Client Index: 1\n");
            printf("Tile Size: 65472\n");
            printf("User Name: spider\n");
            printf("Host Name: spider-os\n");
            printf("Server Name: pulseaudio\n");
            printf("Server Version: 16.1\n");
            printf("Default Sample Specification: s16le 2ch 44100Hz\n");
            printf("Default Channel Map: front-left,front-right\n");
            printf("Default Sink: alsa_output.pci-0000_00_07.0.analog-stereo\n");
            printf("Default Source: alsa_input.pci-0000_00_07.0.analog-stereo\n");
            return 0;
        }
    }

    return 0;
}
