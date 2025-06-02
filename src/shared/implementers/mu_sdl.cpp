/*
    LGCK Builder Runtime
    Copyright (C) 1999, 2020  Francois Blanchette

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <cstdio>
extern "C"
{
#include <SDL2/SDL.h>
#include <xmp.h>
}
#include "mu_sdl.h"

#define AUDIO_RATE 48000
#define AUDIO_FORMAT AUDIO_S16SYS
#define AUDIO_CHANNELS 2
#define AUDIO_SAMPLES 1024

xmp_context ctx;
SDL_AudioDeviceID dev;
struct xmp_frame_info info;

CMusicSDL::CMusicSDL()
{
    m_valid = true;
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0)
    {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        m_valid = false;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) == -1)
    {
        fprintf(stderr, "Mix_OpenAudio Error: %s\n", Mix_GetError());
        m_valid = false;
    }

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = AUDIO_RATE;
    spec.format = AUDIO_FORMAT;
    spec.channels = AUDIO_CHANNELS;
    spec.samples = AUDIO_SAMPLES;
    spec.callback = NULL;
    SDL_AudioSpec obtained;
    dev = SDL_OpenAudioDevice(NULL, 0, &spec, &obtained, 0);
    if (dev == 0)
    {
        fprintf(stderr, "SDL_OpenAudioDevice error: %s\n", SDL_GetError());
    }

    if (SDL_OpenAudio(&spec, NULL) < 0)
    {
        fprintf(stderr, "SDL_OpenAudio Error: %s\n", SDL_GetError());
    }

    // Initialize libxmp
    ctx = xmp_create_context();

    // Make sure XMP is enabled
    if (!(Mix_Init(MIX_INIT_MOD) & MIX_INIT_MOD))
    {
        printf("Mix_Init MOD error: %s\n", Mix_GetError());
    }

    m_music = nullptr;
}

CMusicSDL::~CMusicSDL()
{
    close();

    xmp_end_player(ctx);
    xmp_release_module(ctx);
    xmp_free_context(ctx);
}

bool CMusicSDL::open(const char *file)
{
    printf("opening music: %s\n", file);

    close();
    if (m_valid)
    {
        // m_music = Mix_LoadMUS(file);
        // if (!m_music)
        //{
        //    printf("Failed to load music: %s\n", Mix_GetError());
        //}

        if (xmp_load_module(ctx, file) != 0)
        {
            fprintf(stderr, "Error: could not load module %s\n", file);
            // xmp_free_context(ctx);
            return false;
        }

        if (xmp_start_player(ctx, AUDIO_RATE, 0) != 0)
        {
            fprintf(stderr, "Error: could not start player\n");
            //   xmp_free_context(ctx);
            return false;
        }

        while (1)
        {
            if (xmp_play_frame(ctx) != 0)
                break;
            xmp_get_frame_info(ctx, &info);

            if (info.loop_count > 0)
                break;

            // Queue audio to SDL2
            SDL_QueueAudio(dev, info.buffer, info.buffer_size);

            SDL_Delay(info.total_time / info.num_rows / 2); // avoid CPU spin
        }
    }
    else
    {
        printf("Mix_LoadMUS(\"%s\"): %s\n", file, Mix_GetError());
    }

    if (m_music == nullptr)
    {
        // printf("m_music is null\n");
    }
    return m_music != nullptr;
}

bool CMusicSDL::play(int loop)
{
    //  if (m_music)
    {
        printf("playing music\n");
        // Mix_PlayMusic(m_music, loop);
        xmp_restart_module(ctx);
        if (xmp_start_player(ctx, AUDIO_RATE, 0) != 0)
        {
            fprintf(stderr, "Error: could not start player\n");
            xmp_free_context(ctx);
            return false;
        }
        return true;
    }
    return false;
}

void CMusicSDL::stop()
{
    // SDL_CloseAudio();
    //  if (m_valid)
    {
        printf("music paused\n");
        SDL_PauseAudio(0);
        Mix_HaltMusic();
    }
    xmp_stop_module(ctx);
}

void CMusicSDL::close()
{
    stop();
    if (m_music)
    {

        SDL_CloseAudio();
        // Mix_FreeMusic(m_music);
        m_music = nullptr;
    }
}

bool CMusicSDL::isValid()
{
    return m_valid;
}

const char *CMusicSDL::signature() const
{
    return "lgck-music-sdl";
}
