/*
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "eglutil.h"

#include <SDL2/SDL.h>

struct sdl_test {
    uint32_t width;
    uint32_t height;
    uint32_t win_flags;

    SDL_Window *win;
    SDL_GLContext ctx;

    PFNGLCLEARCOLORPROC ClearColor;
    PFNGLCLEARPROC Clear;
};

static void
sdl_log_event_windowevent(const SDL_Event *ev)
{
    switch (ev->window.event) {
#define CASE(ty)                                                                                 \
    case ty:                                                                                     \
        egl_log("  " #ty);                                                                       \
        break
        CASE(SDL_WINDOWEVENT_SHOWN);
        CASE(SDL_WINDOWEVENT_HIDDEN);
        CASE(SDL_WINDOWEVENT_EXPOSED);
        CASE(SDL_WINDOWEVENT_MOVED);
        CASE(SDL_WINDOWEVENT_RESIZED);
        CASE(SDL_WINDOWEVENT_SIZE_CHANGED);
        CASE(SDL_WINDOWEVENT_MINIMIZED);
        CASE(SDL_WINDOWEVENT_MAXIMIZED);
        CASE(SDL_WINDOWEVENT_RESTORED);
        CASE(SDL_WINDOWEVENT_ENTER);
        CASE(SDL_WINDOWEVENT_LEAVE);
        CASE(SDL_WINDOWEVENT_FOCUS_GAINED);
        CASE(SDL_WINDOWEVENT_FOCUS_LOST);
        CASE(SDL_WINDOWEVENT_CLOSE);
        CASE(SDL_WINDOWEVENT_TAKE_FOCUS);
        CASE(SDL_WINDOWEVENT_HIT_TEST);
        CASE(SDL_WINDOWEVENT_ICCPROF_CHANGED);
        CASE(SDL_WINDOWEVENT_DISPLAY_CHANGED);
#undef CASE
    default:
        egl_log("unknown windowe vent 0x%x", ev->window.event);
        break;
    }
}

static void
sdl_log_event(const SDL_Event *ev)
{
    switch (ev->type) {
#define CASE(ty)                                                                                 \
    case ty:                                                                                     \
        egl_log(#ty);                                                                            \
        break
        CASE(SDL_QUIT);
        CASE(SDL_APP_TERMINATING);
        CASE(SDL_APP_LOWMEMORY);
        CASE(SDL_APP_WILLENTERBACKGROUND);
        CASE(SDL_APP_DIDENTERBACKGROUND);
        CASE(SDL_APP_WILLENTERFOREGROUND);
        CASE(SDL_APP_DIDENTERFOREGROUND);
        CASE(SDL_LOCALECHANGED);
        CASE(SDL_DISPLAYEVENT);
        CASE(SDL_WINDOWEVENT);
        CASE(SDL_SYSWMEVENT);
        CASE(SDL_KEYDOWN);
        CASE(SDL_KEYUP);
        CASE(SDL_TEXTEDITING);
        CASE(SDL_TEXTINPUT);
        CASE(SDL_KEYMAPCHANGED);
        CASE(SDL_TEXTEDITING_EXT);
        CASE(SDL_MOUSEMOTION);
        CASE(SDL_MOUSEBUTTONDOWN);
        CASE(SDL_MOUSEBUTTONUP);
        CASE(SDL_MOUSEWHEEL);
        CASE(SDL_JOYAXISMOTION);
        CASE(SDL_JOYBALLMOTION);
        CASE(SDL_JOYHATMOTION);
        CASE(SDL_JOYBUTTONDOWN);
        CASE(SDL_JOYBUTTONUP);
        CASE(SDL_JOYDEVICEADDED);
        CASE(SDL_JOYDEVICEREMOVED);
        CASE(SDL_JOYBATTERYUPDATED);
        CASE(SDL_CONTROLLERAXISMOTION);
        CASE(SDL_CONTROLLERBUTTONDOWN);
        CASE(SDL_CONTROLLERBUTTONUP);
        CASE(SDL_CONTROLLERDEVICEADDED);
        CASE(SDL_CONTROLLERDEVICEREMOVED);
        CASE(SDL_CONTROLLERDEVICEREMAPPED);
        CASE(SDL_CONTROLLERTOUCHPADDOWN);
        CASE(SDL_CONTROLLERTOUCHPADMOTION);
        CASE(SDL_CONTROLLERTOUCHPADUP);
        CASE(SDL_CONTROLLERSENSORUPDATE);
        CASE(SDL_FINGERDOWN);
        CASE(SDL_FINGERUP);
        CASE(SDL_FINGERMOTION);
        CASE(SDL_DOLLARGESTURE);
        CASE(SDL_DOLLARRECORD);
        CASE(SDL_MULTIGESTURE);
        CASE(SDL_CLIPBOARDUPDATE);
        CASE(SDL_DROPFILE);
        CASE(SDL_DROPTEXT);
        CASE(SDL_DROPBEGIN);
        CASE(SDL_DROPCOMPLETE);
        CASE(SDL_AUDIODEVICEADDED);
        CASE(SDL_AUDIODEVICEREMOVED);
        CASE(SDL_SENSORUPDATE);
        CASE(SDL_RENDER_TARGETS_RESET);
        CASE(SDL_RENDER_DEVICE_RESET);
        CASE(SDL_POLLSENTINEL);
        CASE(SDL_USEREVENT);
#undef CASE
    default:
        egl_log("unknown event 0x%x", ev->type);
        break;
    }

    switch (ev->type) {
    case SDL_WINDOWEVENT:
        sdl_log_event_windowevent(ev);
        break;
    default:
        break;
    }
}

static void
sdl_test_init(struct sdl_test *test)
{
    if (SDL_Init(SDL_INIT_VIDEO))
        egl_die("failed to init sdl");

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

    test->win = SDL_CreateWindow("test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                 test->width, test->height, test->win_flags);
    if (!test->win)
        egl_die("failed to create win");

    test->ctx = SDL_GL_CreateContext(test->win);

    test->ClearColor = (PFNGLCLEARCOLORPROC)SDL_GL_GetProcAddress("glClearColor");
    test->Clear = (PFNGLCLEARPROC)SDL_GL_GetProcAddress("glClear");
}

static void
sdl_test_cleanup(struct sdl_test *test)
{
    SDL_GL_DeleteContext(test->ctx);
    SDL_DestroyWindow(test->win);
    SDL_Quit();
}

static void
sdl_test_draw(struct sdl_test *test)
{
    while (true) {
        SDL_Event ev;
        if (!SDL_WaitEvent(&ev))
            egl_die("failed to wait event");
        sdl_log_event(&ev);

        bool quit = false;
        bool redraw = false;
        bool toggle_fullscreen = false;
        bool toggle_minimize = false;
        bool toggle_maximize = false;

        switch (ev.type) {
        case SDL_QUIT:
            quit = true;
            break;
        case SDL_WINDOWEVENT:
            redraw = ev.window.event == SDL_WINDOWEVENT_EXPOSED;
            break;
        case SDL_KEYUP:
            switch (ev.key.keysym.sym) {
            case SDLK_f:
                toggle_fullscreen = true;
                break;
            case SDLK_m:
                if (ev.key.keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
                    toggle_maximize = true;
                else
                    toggle_minimize = true;
                break;
            case SDLK_q:
                quit = true;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }

        if (quit)
            break;

        if (toggle_fullscreen) {
            const uint32_t win_flags = SDL_GetWindowFlags(test->win);
            const uint32_t fs_flags =
                win_flags & SDL_WINDOW_FULLSCREEN ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP;
            SDL_SetWindowFullscreen(test->win, fs_flags);
        }

        if (toggle_minimize) {
            const uint32_t win_flags = SDL_GetWindowFlags(test->win);
            if (win_flags & SDL_WINDOW_MINIMIZED)
                SDL_RestoreWindow(test->win);
            else
                SDL_MinimizeWindow(test->win);
        }

        if (toggle_maximize) {
            const uint32_t win_flags = SDL_GetWindowFlags(test->win);
            if (win_flags & SDL_WINDOW_MAXIMIZED)
                SDL_RestoreWindow(test->win);
            else
                SDL_MaximizeWindow(test->win);
        }

        if (redraw) {
#if 0
            SDL_Surface *surf = SDL_GetWindowSurface(test->win);
            const uint32_t color = SDL_MapRGB(surf->format, 0xff, 0x80, 0x80);
            SDL_FillRect(surf, NULL, color);
            SDL_UpdateWindowSurface(test->win);
#else
            test->ClearColor(1.0f, 0.5f, 0.5f, 1.0f);
            test->Clear(GL_COLOR_BUFFER_BIT);
            SDL_GL_SwapWindow(test->win);
#endif
        }
    }
}

int
main(void)
{
    struct sdl_test test = {
        .width = 320,
        .height = 240,
        .win_flags = SDL_WINDOW_OPENGL,
    };

    sdl_test_init(&test);
    sdl_test_draw(&test);
    sdl_test_cleanup(&test);

    return 0;
}
