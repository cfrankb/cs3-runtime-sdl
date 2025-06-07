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
#include "../xm/modplay.h"
#include "../FileWrap.h"

extern AUDIOPLAYER Audioplayer;

#ifdef __EMSCRIPTEN__
void playXM(void *userData)
{
    if (CMusicSDL::type() != CMusicSDL::TYPE_NONE &&
        CMusicSDL::isPlaying())
    {
        PlayMusic();
    }
}
#else
pthread_t thread1;
void *playXMThread(void *)
{
    while (1)
    {
        if (CMusicSDL::type() != CMusicSDL::TYPE_NONE &&
            CMusicSDL::isPlaying())
        {
            PlayMusic();
        }
        SDL_Delay(20);
    }
    return nullptr;
}
#endif

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

    // Init XM Player
    if (InitAudioplayerStruct() != 0)
    {
        SDL_Log("can not init audio!");
    }

#ifndef __EMSCRIPTEN__
    if (Mix_OpenAudio(22050, AUDIO_S16SYS, 2, 8192) < 0)
    {
        fprintf(stderr, "Mix_OpenAudio failed: %s\n", SDL_GetError());
        m_valid = false;
    }

    // Init SDL Audio for OGG
    auto mixMod = MIX_INIT_OGG;
    if (!(Mix_Init(mixMod) & mixMod))
    {
        fprintf(stderr, "Mix_Init MOD error: %s\n", Mix_GetError());
    }

    pthread_create(&thread1, nullptr, &playXMThread, nullptr);
#endif
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
    if (strstr(file, ".ogg"))
    {
        m_data.mixData = Mix_LoadMUS(file);
        if (!m_data.mixData)
        {
            fprintf(stderr, "Failed to load music: %s\n", Mix_GetError());
        }
        else
        {
            m_type = TYPE_OGG;
            valid = true;
        }
    }
    else if (strstr(file, ".xm"))
    {
        CFileWrap wrap;
        if (wrap.open(file, "rb"))
        {
            int size = wrap.getSize();
            m_data.xmData = new uint8_t[size];
            wrap.read(m_data.xmData, size);
            wrap.close();
            Audioplayer.pMusicStart = m_data.xmData;
            Audioplayer.nMusicSize = size;
            if (SetMusic(0) != 0)
            {
                SDL_Log("can not init song data!");
            }
            else
            {
                m_type = TYPE_XM;
                valid = true;
            }
        }
        else
        {
            fprintf(stderr, "Failed to opem music: %s\n", file);
        }
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
    else if (m_type == TYPE_XM)
    {
        // nothing to do
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
    else if (m_type == TYPE_XM)
    {
        delete[] m_data.xmData;
        m_data.xmData = nullptr;
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
