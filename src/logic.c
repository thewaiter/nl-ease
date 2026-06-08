#include <stdlib.h>
#include <stdio.h>
#include <Ecore.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>          

#include "logic.h"
#include "xrandr.h"

static AppState state;

static Eina_Bool daemon_timer_cb(void *data);
static int       is_in_schedule(void);

/* Internal Functions */

static int
is_in_schedule(void)
{
    time_t t = time(NULL);
    const struct tm *tm = localtime(&t);

    int now_minutes   = tm->tm_hour * 60 + tm->tm_min;
    int start_minutes = state.start_hour * 60;
    int end_minutes   = state.end_hour * 60;

    if (state.start_hour < state.end_hour) {
        return (now_minutes >= start_minutes && now_minutes < end_minutes);
    } else {
        return (now_minutes >= start_minutes || now_minutes < end_minutes);
    }
}

void
logic_apply(void)
{
    int in_schedule = is_in_schedule();

    /* Logging every 5 min */
    static time_t last_log = 0;
    time_t now = time(NULL);

    if (now - last_log >= 300)
     {
        const struct tm *tm = localtime(&now);

        printf("[nl-ease %02d:%02d] enabled=%d | %02d:00-%02d:00 | in_schedule=%d → %s\n",
               tm->tm_hour, tm->tm_min,
               state.enabled,
               state.start_hour, state.end_hour,
               in_schedule,
               (state.enabled && in_schedule) ? "NIGHT MODE" : "DAY MODE");
        last_log = now;
     }

    if (state.enabled && in_schedule) {
        xrandr_set_temperature(state.temperature);
    } else {
        xrandr_reset();
    }
}

void
logic_init(void)
{
    state.enabled = 0;
    state.temperature = 4500;
    state.start_hour = 22;
    state.end_hour = 6;

    logic_load();
    logic_apply();
}

AppState *
logic_get_state(void)
{
    return &state;
}

void
logic_set_enabled(int enabled)
{
    state.enabled = enabled;
    logic_apply();
    logic_save();
}

void
logic_set_temperature(int temp)
{
    state.temperature = temp;
    logic_apply();
    logic_save();
}

void
logic_set_schedule(int start_hour, int end_hour)
{
    state.start_hour = start_hour;
    state.end_hour = end_hour;
    logic_apply();
    logic_save();
}

/* DAEMON */

static Eina_Bool
daemon_timer_cb(void *data EINA_UNUSED)
{
    static int load_counter = 0;

    /* read conf every 60 sec */
    if (++load_counter >= 4) {   // 4*15s=60s
        logic_load();
        load_counter = 0;
    }

    logic_apply();
    return ECORE_CALLBACK_RENEW;
}

void
logic_run_daemon(void)
{
    logic_init();

    ecore_timer_add(15.0, daemon_timer_cb, NULL);

    printf("nl-ease daemon started (PID %d)\n", getpid());
    printf("Night light schedule: %02d:00 - %02d:00\n", state.start_hour, state.end_hour);

    ecore_main_loop_begin();

    printf("nl-ease daemon exiting. Resetting display...\n");
    xrandr_reset();
}

/* CONFIG */

static const char *
get_config_path(void)
{
    static char path[256];
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";

    snprintf(path, sizeof(path), "%s/.config/nl-ease.conf", home);
    return path;
}

void
logic_save(void)
{
    const char *home = getenv("HOME");
    if (!home) return;

    char dir[256];
    snprintf(dir, sizeof(dir), "%s/.config", home);
    mkdir(dir, 0755);

    FILE *f = fopen(get_config_path(), "w");
    if (!f) return;

    fprintf(f, "enabled=%d\n", state.enabled);
    fprintf(f, "temperature=%d\n", state.temperature);
    fprintf(f, "start=%d\n", state.start_hour);
    fprintf(f, "end=%d\n", state.end_hour);

    fclose(f);
}

void
logic_load(void)
{
    FILE *f = fopen(get_config_path(), "r");
    if (!f) return;

    fscanf(f, "enabled=%d\n", &state.enabled);
    fscanf(f, "temperature=%d\n", &state.temperature);
    fscanf(f, "start=%d\n", &state.start_hour);
    fscanf(f, "end=%d\n", &state.end_hour);

    fclose(f);
}
