#define _GNU_SOURCE

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>

#define SCHEDULER_NAME "com.system76.Scheduler"
#define SCHEDULER_PATH "/com/system76/Scheduler"
#define SCHEDULER_INTERFACE "com.system76.Scheduler"
#define STEAM_HOME_APP_ID 769U

struct focus {
    bool usable;
    uint32_t app_id;
    uint32_t pid;
};

struct state {
    sd_bus *bus;
    struct focus current;
    bool foreground_set;
    uint32_t last_pid;
};

static volatile sig_atomic_t stopping;

static void stop_handler(int signal_number)
{
    (void)signal_number;
    stopping = 1;
}

static bool read_property(Display *display, Window root, Atom atom, unsigned long **values, unsigned long *count)
{
    Atom type;
    int format;
    unsigned long bytes_after;
    unsigned char *data = NULL;
    int status = XGetWindowProperty(display, root, atom, 0, 0x7fffffff, False, AnyPropertyType,
                                    &type, &format, count, &bytes_after, &data);
    if (status != Success || !data || format != 32) {
        if (data)
            XFree(data);
        return false;
    }
    *values = (unsigned long *)data;
    return true;
}

static struct focus resolve_focus(Display *display, Window root, Atom focused_atom, Atom focusable_atom)
{
    struct focus result = {0};
    unsigned long *focused = NULL;
    unsigned long focused_count = 0;
    unsigned long *focusable = NULL;
    unsigned long focusable_count = 0;

    if (!read_property(display, root, focused_atom, &focused, &focused_count)
        || focused_count == 0) {
        if (focused)
            XFree(focused);
        return result;
    }

    Window focused_window = (Window)focused[0];
    XFree(focused);
    if (!focused_window || !read_property(display, root, focusable_atom, &focusable, &focusable_count)) {
        if (focusable)
            XFree(focusable);
        return result;
    }

    for (unsigned long i = 0; i + 2 < focusable_count; i += 3) {
        if ((Window)focusable[i] != focused_window)
            continue;
        result.app_id = (uint32_t)focusable[i + 1];
        result.pid = (uint32_t)focusable[i + 2];
        result.usable = result.pid != 0 && result.app_id != STEAM_HOME_APP_ID;
        break;
    }
    XFree(focusable);
    return result;
}

static int scheduler_call(sd_bus *bus, const char *method, uint32_t pid)
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    int result;

    if (pid)
        result = sd_bus_call_method(bus, SCHEDULER_NAME, SCHEDULER_PATH, SCHEDULER_INTERFACE,
                                    method, &error, &reply, "u", pid);
    else
        result = sd_bus_call_method(bus, SCHEDULER_NAME, SCHEDULER_PATH, SCHEDULER_INTERFACE,
                                    method, &error, &reply, "");
    if (result < 0)
        fprintf(stderr, "gamescope companion: %s failed: %s\n", method,
                error.message ? error.message : strerror(-result));
    else
        if (pid)
            fprintf(stderr, "gamescope companion: %s succeeded for PID %u\n", method, pid);
        else
            fprintf(stderr, "gamescope companion: %s succeeded\n", method);
    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
    return result;
}

static int set_foreground(sd_bus *bus, struct focus focus, bool *set, uint32_t *last_pid, bool force)
{
    if (!focus.usable) {
        if (!*set)
            return 0;
        int result = scheduler_call(bus, "ClearForeground", 0);
        if (result >= 0)
            *set = false;
        return result;
    }
    if (!force && *set && *last_pid == focus.pid)
        return 0;
    int result = scheduler_call(bus, "SetForegroundProcess", focus.pid);
    if (result >= 0) {
        *set = true;
        *last_pid = focus.pid;
    }
    return result;
}

static int set_foreground_state(struct state *state, bool force)
{
    return set_foreground(state->bus, state->current, &state->foreground_set, &state->last_pid, force);
}

static int owner_changed(sd_bus_message *message, void *userdata, sd_bus_error *error)
{
    (void)error;
    struct state *state = userdata;
    const char *name, *old_owner, *new_owner;
    if (sd_bus_message_read(message, "sss", &name, &old_owner, &new_owner) < 0
        || strcmp(name, SCHEDULER_NAME) != 0 || !new_owner[0])
        return 0;

    /* The scheduler may have restarted; restore the current target immediately. */
    set_foreground_state(state, true);
    return 0;
}

static int probe(Display *display, Window root, Atom focused_atom, Atom focusable_atom)
{
    struct focus focus = resolve_focus(display, root, focused_atom, focusable_atom);
    if (!focus.usable) {
        puts("no usable game focus (Steam Home, missing PID, or no matching window)");
        return 1;
    }
    printf("focused game: app_id=%u pid=%u\n", focus.app_id, focus.pid);
    return 0;
}

int main(int argc, char **argv)
{
    bool probe_only = argc == 2 && strcmp(argv[1], "--probe") == 0;
    if (argc > 1 && !probe_only) {
        fprintf(stderr, "usage: %s [--probe]\n", argv[0]);
        return 2;
    }

    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "gamescope companion: cannot open DISPLAY\n");
        return 1;
    }
    Window root = DefaultRootWindow(display);
    Atom focused_atom = XInternAtom(display, "GAMESCOPE_FOCUSED_WINDOW", False);
    Atom focusable_atom = XInternAtom(display, "GAMESCOPE_FOCUSABLE_WINDOWS", False);

    if (probe_only) {
        int result = probe(display, root, focused_atom, focusable_atom);
        XCloseDisplay(display);
        return result;
    }

    sd_bus *bus = NULL;
    if (sd_bus_open_system(&bus) < 0) {
        fprintf(stderr, "gamescope companion: cannot connect to the system bus\n");
        XCloseDisplay(display);
        return 1;
    }
    XSelectInput(display, root, PropertyChangeMask);
    XFlush(display);
    struct sigaction action = {.sa_handler = stop_handler};
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    sd_bus_slot *owner_slot = NULL;
    struct state state = {.bus = bus};
    sd_bus_match_signal(bus, &owner_slot, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                        "org.freedesktop.DBus", "NameOwnerChanged", owner_changed, &state);
    state.current = resolve_focus(display, root, focused_atom, focusable_atom);
    set_foreground_state(&state, true);

    int x_fd = ConnectionNumber(display);
    while (!stopping) {
        while (sd_bus_process(bus, NULL) > 0) {
            /* Drain queued D-Bus messages, including owner changes. */
        }
        struct pollfd fds[2] = {{.fd = x_fd, .events = POLLIN}, {.fd = sd_bus_get_fd(bus), .events = POLLIN}};
        if (poll(fds, 2, 500) < 0)
            continue;
        if (fds[1].revents & POLLIN) {
            /* A new scheduler owner requires an unconditional resend. */
            /* NameOwnerChanged above performs the resend. */
        }
        if (fds[0].revents & POLLIN) {
            while (XPending(display)) {
                XEvent event;
                XNextEvent(display, &event);
                if (event.type == PropertyNotify
                    && (event.xproperty.atom == focused_atom || event.xproperty.atom == focusable_atom)) {
                    state.current = resolve_focus(display, root, focused_atom, focusable_atom);
                    set_foreground_state(&state, false);
                }
            }
        }
    }
    if (state.foreground_set)
        scheduler_call(bus, "ClearForeground", 0);
    sd_bus_slot_unref(owner_slot);
    sd_bus_unref(bus);
    XCloseDisplay(display);
    return 0;
}
