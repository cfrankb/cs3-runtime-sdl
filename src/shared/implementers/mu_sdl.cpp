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
#include <unistd.h>
#include "mu_sdl.h"
#include "SDL2/SDL.h"
#include "../FileWrap.h"

uint8_t CMusicSDL::m_type = static_cast<uint8_t>(CMusicSDL::TYPE_NONE);
bool CMusicSDL::m_playing = false;

CMusicSDL::CMusicSDL()
{
    m_type = TYPE_NONE;
    m_data.mixData = nullptr;
    m_data.xmData = nullptr;

    m_valid = true;
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        fprintf(stderr, "SDL_init failed: %s\n", SDL_GetError());
    }

    if (Mix_OpenAudio(22050, AUDIO_S16SYS, 2, 8192) < 0)
    {
        fprintf(stderr, "Mix_OpenAudio failed: %s\n", SDL_GetError());
        m_valid = false;
    }

    // Init SDL Audio for OGG
    auto mixMod = MIX_INIT_OGG | MIX_INIT_MOD;
    if (!(Mix_Init(mixMod) & mixMod))
    {
        fprintf(stderr, "Mix_Init MOD error: %s\n", Mix_GetError());
    }
}

CMusicSDL::~CMusicSDL()
{
    close();
}

bool CMusicSDL::open(const char *file)
{
    printf("opening music: %s\n", file);

    if (m_type != TYPE_NONE)
    {
        close();
    }
    bool valid = false;

    m_data.mixData = Mix_LoadMUS(file);
    if (m_data.mixData)
    {
        m_type = TYPE_OGG;
        valid = true;
    }
    else
    {
        m_type = TYPE_NONE;
        fprintf(stderr, "Failed to load music: %s\n", Mix_GetError());
    }

    return valid;
}
bool CMusicSDL::play(int loop)
{
    m_playing = true;
    if (m_type == TYPE_OGG)
    {
        printf("playing music\n");
        Mix_PlayMusic(m_data.mixData, loop);
        return true;
    }
    return false;
}

void CMusicSDL::stop()
{
    if (m_type == TYPE_OGG)
    {
        Mix_HaltMusic();
    }
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

void CMusicSDL::setVolume(int v)
{
    // MIX_MAX_VOLUME 128
    Mix_VolumeMusic(v);
}