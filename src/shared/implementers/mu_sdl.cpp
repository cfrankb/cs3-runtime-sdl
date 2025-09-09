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

#include <cstring>
#include <cstdio>
#include <unistd.h>
#include "mu_sdl.h"

#include "../FileWrap.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
static bool endswith(const char *str, const char *end)
{
    const char *t = strstr(str, end);
    return t && strcmp(t, end) == 0;
}
#endif

CMusicSDL::CMusicSDL()
{
    m_type = TYPE_NONE;
    m_data.mixData = nullptr;
    m_data.xmData = nullptr;
    m_valid = false;

    if (!SDL_Init(SDL_INIT_AUDIO))
    {
        fprintf(stderr, "SDL_init failed: %s\n", SDL_GetError());
        return;
    }

    // Open default audio device
    m_data.audioDevice = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!m_data.audioDevice)
    {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return;
    }

    // Initialize SDL_mixer with the audio device
    if (!Mix_OpenAudio(m_data.audioDevice, nullptr))
    {
        fprintf(stderr, "Mix_OpenAudioDevice failed: %s\n", SDL_GetError());
        return;
    }
    m_valid = true;
}

CMusicSDL::~CMusicSDL()
{
    close();
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
bool CMusicSDL::open(const char *file)
{
    if (m_type != TYPE_NONE)
    {
        close();
    }
    bool valid = false;

#ifdef __EMSCRIPTEN__
    if (!endswith(file, ".xm"))
    {
        m_filepath = file;
        m_type = TYPE_STREAM;
        return true;
    }
#endif
    m_data.mixData = Mix_LoadMUS(file);
    if (m_data.mixData)
    {
        m_type = TYPE_OGG;
        valid = true;
    }
    else
    {
        m_type = TYPE_NONE;
        fprintf(stderr, "Failed to load music `%s` : %s\n", file, SDL_GetError());
    }
    return valid;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
bool CMusicSDL::play(int loop)
{
    m_playing = true;
    if (m_type == TYPE_OGG)
    {
        Mix_PlayMusic(m_data.mixData, loop);
        return true;
    }
#ifdef __EMSCRIPTEN__
    else if (m_type == TYPE_STREAM)
    {
        EM_ASM({
            const music = UTF8ToString($0);
            startAudio(music); }, m_filepath.c_str());
        return true;
    }
#endif
    return false;
}

void CMusicSDL::stop()
{
    if (m_type == TYPE_OGG)
    {
        Mix_HaltMusic();
    }
#ifdef __EMSCRIPTEN__
    else if (m_type == TYPE_STREAM)
    {
        EM_ASM({
            stopAudio();
        });
    }
#endif
    m_playing = false;
}

void CMusicSDL::close()
{
    stop();
    SDL_Delay(100);
    if (m_type == TYPE_OGG)
    {
        Mix_FreeMusic(m_data.mixData);
        m_data.mixData = nullptr;
    }
    else if (m_type == TYPE_STREAM)
    {
        m_filepath = "";
    }
    m_type = TYPE_NONE;
}

bool CMusicSDL::isValid()
{
    return m_valid;
}

const char *CMusicSDL::signature() const
{
    return "lgck-music-sdl";
}

int CMusicSDL::getVolume()
{
    return Mix_VolumeMusic(-1);
}

void CMusicSDL::setVolume(int value)
{
    // MIX_MAX_VOLUME 128
    Mix_VolumeMusic(value);
#ifdef __EMSCRIPTEN__
    EM_ASM({ setVolume($0); }, value);
#endif
}