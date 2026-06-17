#include <Elementary.h>
#include <libintl.h>
#include <unistd.h>
#include <Ecore.h>          
#include "logic.h"

#define _(String) gettext(String)

static Evas_Object *start_spinner;
static Evas_Object *end_spinner;

static void
kill_daemon(void)
{
    char cmd[256];
    pid_t my_pid = getpid();

    snprintf(cmd, sizeof(cmd),
             "pgrep -f 'nl-ease --daemon' | grep -v %d | xargs -r kill -TERM 2>/dev/null || true", 
             my_pid);

    ecore_exe_run(cmd, NULL);
    usleep(150000);  // 150ms

    snprintf(cmd, sizeof(cmd),
             "pgrep -f 'nl-ease --daemon' | grep -v %d | xargs -r kill -KILL 2>/dev/null || true", 
             my_pid);
    ecore_exe_run(cmd, NULL);

    printf("Daemon termination attempted.\n");
}

static void
on_toggle_changed(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
    int enabled = elm_check_state_get(obj);
    
    logic_set_enabled(enabled);

    if (enabled == 0) {
        printf("Enabled -> OFF: killing daemon...\n");
        kill_daemon();
    } else {
        printf("Enabled -> ON\n");
    }
}

static void
on_slider_changed(void *data EINA_UNUSED, Evas_Object *obj, void *event_info EINA_UNUSED)
{
    int visual_value = (int)elm_slider_value_get(obj);
    int real_temperature = 9000 - visual_value;
    logic_set_temperature(real_temperature);
}

static void
on_schedule_changed(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
    int start = (int)elm_spinner_value_get(start_spinner);
    int end   = (int)elm_spinner_value_get(end_spinner);

    logic_set_schedule(start, end);
}

static void
on_save_clicked(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
    logic_save();
}

static void
on_launch_daemon_clicked(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
    // first time config save
    logic_save();

    // close window
    Evas_Object *win = (Evas_Object *)data;
    if (win)
        evas_object_del(win);

    // launch daemon
    ecore_exe_run("nl-ease --daemon &", NULL);

    printf("Daemon launched. Closing GUI...\n");
    elm_exit();
}

static void
on_win_delete(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
    const AppState *s = logic_get_state();
    
    if (!s->enabled) {
        printf("Closing GUI with Enabled=OFF -> killing daemon\n");
        kill_daemon();
    }
    
    elm_exit();
}

void
ui_init(void)
{
    Evas_Object *win = elm_win_util_standard_add("nl-ease", "nl-ease");
    elm_win_autodel_set(win, EINA_TRUE);

    evas_object_resize(win, 300, 280);
    evas_object_size_hint_min_set(win, 300, 280);
    evas_object_size_hint_max_set(win, 300, 280);
    
    elm_win_size_base_set(win, 300, 280);
    elm_win_size_step_set(win, 0, 0);

    Evas_Object *box = elm_box_add(win);
    evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    elm_win_resize_object_add(win, box);
    elm_box_padding_set(box, 0, 6);
    evas_object_show(box);

    // Toggle Enabled
    Evas_Object *toggle = elm_check_add(win);
    elm_object_text_set(toggle, _("Enabled"));
    evas_object_smart_callback_add(toggle, "changed", on_toggle_changed, NULL);
    elm_box_pack_end(box, toggle);
    evas_object_show(toggle);

    // Temperature Slider 
    Evas_Object *slider = elm_slider_add(win);
    elm_slider_min_max_set(slider, 2500, 6500);
    elm_slider_value_set(slider, 4500);
    elm_object_text_set(slider, _("Temperature"));
    evas_object_smart_callback_add(slider, "changed", on_slider_changed, NULL);
    evas_object_size_hint_min_set(slider, 220, -1);
    elm_box_pack_end(box, slider);
    evas_object_show(slider);

    // Start time
    Evas_Object *start_label = elm_label_add(win);
    elm_object_text_set(start_label, _("Start time:"));
    elm_box_pack_end(box, start_label);
    evas_object_show(start_label);

    start_spinner = elm_spinner_add(win);
    elm_spinner_min_max_set(start_spinner, 0, 23);
    elm_spinner_value_set(start_spinner, 22);
    evas_object_smart_callback_add(start_spinner, "changed", on_schedule_changed, NULL);
    elm_box_pack_end(box, start_spinner);
    evas_object_show(start_spinner);

    // End time
    Evas_Object *end_label = elm_label_add(win);
    elm_object_text_set(end_label, _("End time:"));
    elm_box_pack_end(box, end_label);
    evas_object_show(end_label);

    end_spinner = elm_spinner_add(win);
    elm_spinner_min_max_set(end_spinner, 0, 23);
    elm_spinner_value_set(end_spinner, 6);
    evas_object_smart_callback_add(end_spinner, "changed", on_schedule_changed, NULL);
    elm_box_pack_end(box, end_spinner);
    evas_object_show(end_spinner);

    // Buttons container
    Evas_Object *btn_box = elm_box_add(win);
    elm_box_horizontal_set(btn_box, EINA_FALSE);
    elm_box_homogeneous_set(btn_box, EINA_TRUE);
    elm_box_padding_set(btn_box, 0, 8);
    evas_object_size_hint_weight_set(btn_box, EVAS_HINT_EXPAND, 0);
    elm_box_pack_end(box, btn_box);
    evas_object_show(btn_box);

    // Save Configuration
    Evas_Object *btn_save = elm_button_add(win);
    elm_object_text_set(btn_save, _("Save Configuration"));
    evas_object_smart_callback_add(btn_save, "clicked", on_save_clicked, NULL);
    elm_box_pack_end(btn_box, btn_save);
    evas_object_show(btn_save);

    // Close and Launch Daemon
    Evas_Object *btn_daemon = elm_button_add(win);
    elm_object_text_set(btn_daemon, _("Close and Launch Daemon"));
    evas_object_smart_callback_add(btn_daemon, "clicked", on_launch_daemon_clicked, win);
    elm_box_pack_end(btn_box, btn_daemon);
    evas_object_show(btn_daemon);

    // Sync UI with current state
    AppState *s = logic_get_state();
    elm_check_state_set(toggle, s->enabled);
    
    int visual_value = 9000 - s->temperature;
    elm_slider_value_set(slider, visual_value);
    
    elm_spinner_value_set(start_spinner, s->start_hour);
    elm_spinner_value_set(end_spinner, s->end_hour);

    // Handle window close with 'X'
    evas_object_smart_callback_add(win, "delete,request", on_win_delete, NULL);

    evas_object_show(win);
}
