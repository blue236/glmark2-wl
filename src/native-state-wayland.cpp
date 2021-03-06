/*
 * Copyright © 2013 Rafal Mielniczuk
 *
 * This file is part of the glmark2 OpenGL (ES) 2.0 benchmark.
 *
 * glmark2 is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * glmark2 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * glmark2.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  Rafal Mielniczuk <rafal.mielniczuk2@gmail.com>
 */

#include "native-state-wayland.h"

#include <cstring>
#include <cstdio>
#include <csignal>
#include <unistd.h>
#include <sys/mman.h>

const struct wl_registry_listener NativeStateWayland::registry_listener_ = {
    NativeStateWayland::registry_handle_global,
    NativeStateWayland::registry_handle_global_remove
};

const struct wl_seat_listener NativeStateWayland::seat_listener_ = {
    NativeStateWayland::seat_handle_capabilities,
    NULL
};

const struct wl_shell_surface_listener NativeStateWayland::shell_surface_listener_ = {
    NativeStateWayland::shell_surface_handle_ping,
    NativeStateWayland::shell_surface_handle_configure,
    NativeStateWayland::shell_surface_handle_popup_done
};

const struct wl_keyboard_listener NativeStateWayland::keyboard_listener_ = {
    NativeStateWayland::keyboard_handle_keymap,
    NativeStateWayland::keyboard_handle_enter,
    NativeStateWayland::keyboard_handle_leave,
    NativeStateWayland::keyboard_handle_key,
    NativeStateWayland::keyboard_handle_modifiers,
    NULL
};

const struct wl_output_listener NativeStateWayland::output_listener_ = {
    NativeStateWayland::output_handle_geometry,
    NativeStateWayland::output_handle_mode,
    NULL,
    NULL
};

volatile bool NativeStateWayland::should_quit_ = false;

NativeStateWayland::NativeStateWayland() : display_(0), window_(0), input_(0)
{
}

NativeStateWayland::~NativeStateWayland()
{
    wl_shell_surface_destroy(window_->shell_surface);
    wl_surface_destroy(window_->surface);
    wl_egl_window_destroy(window_->native);
    delete window_;

    wl_keyboard_destroy(input_->keyboard);
    wl_seat_destroy(input_->seat);
    xkb_state_unref(input_->xkb.state);
    xkb_keymap_unref(input_->xkb.keymap);
    xkb_context_unref(display_->xkb_context);
    delete input_;

    wl_shell_destroy(display_->shell);
    for (size_t i = 0; i < display_->outputs.size(); ++i) {
        wl_output_destroy(display_->outputs.at(i)->output);
        delete display_->outputs.at(i);
    }
    wl_compositor_destroy(display_->compositor);
    wl_registry_destroy(display_->registry);
    wl_display_flush(display_->display);
    wl_display_disconnect(display_->display);
    delete display_;
}

void
NativeStateWayland::registry_handle_global(void *data, struct wl_registry *registry,
                                           uint32_t name, const char *interface, uint32_t version)
{
    uint32_t id = name;
    (void) version;
    NativeStateWayland *that = static_cast<NativeStateWayland *>(data);
    if (strcmp(interface, "wl_compositor") == 0) {
        that->display_->compositor =
                static_cast<struct wl_compositor *>(
                    wl_registry_bind(registry,
                                     id, &wl_compositor_interface, 1));
    } else if (strcmp(interface, "wl_shell") == 0) {
        that->display_->shell =
                static_cast<struct wl_shell *>(
                    wl_registry_bind(registry,
                                     id, &wl_shell_interface, 1));
    } else if (strcmp(interface, "wl_seat") == 0) {
        that->input_->seat =
                static_cast<struct wl_seat *>(
                    wl_registry_bind(registry,
                                     id, &wl_seat_interface, 1));
        wl_seat_add_listener(that->input_->seat, &seat_listener_, that);
    } else if (strcmp(interface, "wl_output") == 0) {
        struct my_output *my_output = new struct my_output;
        memset(my_output, 0, sizeof(*my_output));
        my_output->output =
                static_cast<struct wl_output *>(
                    wl_registry_bind(registry,
                                     id, &wl_output_interface, 1));
        that->display_->outputs.push_back(my_output);

        wl_output_add_listener(my_output->output, &output_listener_, my_output);
        wl_display_roundtrip(that->display_->display);
    }
}

void
NativeStateWayland::registry_handle_global_remove(void *data, struct wl_registry *registry,
                                                  uint32_t name)
{
    (void) data;
    (void) registry;
    (void) name;
}

void
NativeStateWayland::output_handle_geometry(void *data, struct wl_output *wl_output,
         int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
         int32_t subpixel, const char *make, const char *model, int32_t transform)
{
    (void) data;
    (void) wl_output;
    (void) x;
    (void) y;
    (void) physical_width;
    (void) physical_height;
    (void) subpixel;
    (void) make;
    (void) model;
    (void) transform;

}

void
NativeStateWayland::output_handle_mode(void *data, struct wl_output *wl_output, uint32_t flags,
         int32_t width, int32_t height, int32_t refresh)
{
    (void) wl_output;
    (void) flags;
    struct my_output *my_output = static_cast<struct my_output *>(data);
    my_output->width = width;
    my_output->height = height;
    my_output->refresh = refresh;
}

void
NativeStateWayland::shell_surface_handle_ping(void *data, struct wl_shell_surface *shell_surface,
                            uint32_t serial)
{
    (void) data;
    wl_shell_surface_pong(shell_surface, serial);
}

void
NativeStateWayland::shell_surface_handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
    (void) data;
    (void) shell_surface;
}

void
NativeStateWayland::shell_surface_handle_configure(void *data, struct wl_shell_surface *shell_surface,
         uint32_t edges, int32_t width, int32_t height)
{
    (void) shell_surface;
    (void) edges;
    NativeStateWayland *that = static_cast<NativeStateWayland *>(data);
    that->window_->properties.width = width;
    that->window_->properties.height = height;
    wl_egl_window_resize(that->window_->native, width, height, 0, 0);
}

void
NativeStateWayland::keyboard_handle_keymap(void *data, wl_keyboard *wl_keyboard,
                                           uint32_t format, int32_t fd, uint32_t size)
{
    (void) wl_keyboard;
    NativeStateWayland *that = static_cast<NativeStateWayland *>(data);
    char *map_str;

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    map_str = (char*)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (map_str == MAP_FAILED) {
        close(fd);
        return;
    }

    that->input_->xkb.keymap = xkb_map_new_from_string(that->display_->xkb_context,
                            map_str,
                            XKB_KEYMAP_FORMAT_TEXT_V1,
                            (xkb_keymap_compile_flags)0);
    munmap(map_str, size);
    close(fd);

    if (!that->input_->xkb.keymap) {
        fprintf(stderr, "failed to compile keymap\n");
        return;
    }

    that->input_->xkb.state = xkb_state_new(that->input_->xkb.keymap);
    if (!that->input_->xkb.state) {
        fprintf(stderr, "failed to create XKB state\n");
        xkb_map_unref(that->input_->xkb.keymap);
        that->input_->xkb.keymap = NULL;
        return;
    }

    that->input_->xkb.control_mask =
        1 << xkb_map_mod_get_index(that->input_->xkb.keymap, "Control");
    that->input_->xkb.alt_mask =
        1 << xkb_map_mod_get_index(that->input_->xkb.keymap, "Mod1");
    that->input_->xkb.shift_mask =
        1 << xkb_map_mod_get_index(that->input_->xkb.keymap, "Shift");
}

void NativeStateWayland::keyboard_handle_enter(void *data, wl_keyboard *wl_keyboard,
                                               uint32_t serial, wl_surface *surface,
                                               wl_array *keys)
{
    (void) data;
    (void) wl_keyboard;
    (void) serial;
    (void) surface;
    (void) keys;
}

void NativeStateWayland::keyboard_handle_leave(void *data, wl_keyboard *wl_keyboard,
                                               uint32_t serial, wl_surface *surface)
{
    (void) data;
    (void) wl_keyboard;
    (void) serial;
    (void) surface;
}

void NativeStateWayland::keyboard_handle_key(void *data, wl_keyboard *wl_keyboard,
                                             uint32_t serial, uint32_t time, uint32_t key,
                                             uint32_t state)
{
    (void) wl_keyboard;
    (void) serial;
    (void) time;
    (void) state;
    NativeStateWayland *that = static_cast<NativeStateWayland *>(data);
    uint32_t code, num_syms;
    const xkb_keysym_t *syms;
    xkb_keysym_t sym;

    code = key + 8;
    if (!that->input_->xkb.state)
        return;

    num_syms = xkb_key_get_syms(that->input_->xkb.state, code, &syms);

    sym = XKB_KEY_NoSymbol;
    if (num_syms == 1)
        sym = syms[0];

    if (state == WL_KEYBOARD_KEY_STATE_RELEASED && (sym == XKB_KEY_q || sym == XKB_KEY_Q)) {
        that->should_quit_ = true;
    }
}

void NativeStateWayland::keyboard_handle_modifiers(void *data, wl_keyboard *wl_keyboard,
                                                   uint32_t serial, uint32_t mods_depressed,
                                                   uint32_t mods_latched, uint32_t mods_locked,
                                                   uint32_t group)
{
    (void) data;
    (void) wl_keyboard;
    (void) serial;
    (void) mods_depressed;
    (void) mods_latched;
    (void) mods_locked;
    (void) group;
}

void
NativeStateWayland::seat_handle_capabilities(void *data,
                                             struct wl_seat *wl_seat,
                                             uint32_t capabilities)
{
    NativeStateWayland *that = static_cast<NativeStateWayland *>(data);
    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD && !that->input_->keyboard) {
        that->input_->keyboard = wl_seat_get_keyboard(wl_seat);
        wl_keyboard_add_listener(that->input_->keyboard, &that->keyboard_listener_, that);
    }
}

bool
NativeStateWayland::init_display()
{
    display_ = new struct my_display;

    if (!display_) {
        return false;
    }

    display_->display = wl_display_connect(NULL);

    if (!display_->display) {
        return false;
    }

    display_->xkb_context = xkb_context_new((xkb_context_flags)0);

    input_ = new struct my_input;
    input_->keyboard = 0;

    display_->registry = wl_display_get_registry(display_->display);

    wl_registry_add_listener(display_->registry, &registry_listener_, this);

    wl_display_roundtrip(display_->display);

    return true;
}

void*
NativeStateWayland::display()
{
    return static_cast<void *>(display_->display);
}

bool
NativeStateWayland::create_window(WindowProperties const& properties)
{
    struct my_output *output = 0;
    if (!display_->outputs.empty()) output = display_->outputs.at(0);
    window_ = new struct my_window;
    window_->properties = properties;
    window_->surface = wl_compositor_create_surface(display_->compositor);
    if (window_->properties.fullscreen && output) {
        window_->native = wl_egl_window_create(window_->surface, output->width, output->height);
        window_->properties.width = output->width;
        window_->properties.height = output->height;
    } else {
        window_->native = wl_egl_window_create(window_->surface, properties.width, properties.height);
    }
    window_->shell_surface = wl_shell_get_shell_surface(display_->shell, window_->surface);

    if (window_->shell_surface) {
        wl_shell_surface_add_listener(window_->shell_surface,
                                      &shell_surface_listener_, this);
    }

    wl_shell_surface_set_title(window_->shell_surface, "glmark2");

    if (window_->properties.fullscreen) {
        wl_shell_surface_set_fullscreen(window_->shell_surface,
                                        WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER, output->refresh, output->output);
    } else {
        wl_shell_surface_set_toplevel(window_->shell_surface);
    }

    return true;
}

void*
NativeStateWayland::window(WindowProperties &properties)
{
    if (window_) {
        properties = window_->properties;
        return window_->native;
    }

    return 0;
}

void
NativeStateWayland::visible(bool/* v*/)
{
}

bool
NativeStateWayland::should_quit()
{
    return should_quit_;
}

void
NativeStateWayland::flip()
{
    int ret = wl_display_dispatch(display_->display);
    should_quit_ = (ret == -1) || should_quit_;
}

