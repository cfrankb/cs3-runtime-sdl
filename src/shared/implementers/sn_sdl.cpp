/*
    LGCK Builder Runtime
    Copyright (C) 1999, 2014  Francois Blanchette

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
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_mixer.h>
#include "sn_sdl.h"

std::unordered_map<unsigned int, CSndSDL::SND *> m_sounds;

CSndSDL::CSndSDL()
{
    m_valid = false;
    // Initialize all SDL subsystems
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        printf("SDL_init failed: %s\n", SDL_GetError());
        m_valid = false;
    }
    if (SDL_Init(SDL_INIT_EVERYTHING) == -1)
    {
        printf("SDL_init failed: %s\n", SDL_GetError());
        return;
    }
    m_valid = true;
}

CSndSDL::~CSndSDL()
{
    forget();
    SDL_CloseAudio();
}

void CSndSDL::forget()
{
    for (auto it = m_sounds.begin(); it != m_sounds.end(); ++it)
    {
        unsigned int uid = it->first;
        stop(uid);
        SND *snd = it->second;
        // free chunk
        Mix_FreeChunk(snd->chunk);
        // delete snd object
        delete snd;
    }
    m_sounds.clear();
}

bool CSndSDL::add(unsigned char *data, unsigned int size, unsigned int uid)
{
    if (m_sounds.count(uid) != 0)
    {
        printf("ADD: sound already added: %u\n", uid);
        return false;
    }
    SND *snd = new SND;
    for (int i = 0; i < MAX_INSTANCES; ++i)
    {
        snd->channels[i] = -1;
    }
    snd->chunk = nullptr;
    bool fail = false;
    SDL_RWops *rw = SDL_RWFromConstMem(static_cast<void *>(data), size);
    if (!rw)
    {
        fail = true;
        printf("SDL_RWFromConstMem Failed: %s\n", SDL_GetError());
    }
    else
    {
        snd->chunk = Mix_LoadWAV_RW(rw, 1);
        if (snd->chunk)
        {
            Mix_VolumeChunk(snd->chunk, m_volume);
        }
        else
        {
            fail = true;
            printf("Mix_LoadWAV_RW Failed: %s\n", Mix_GetError());
        }
    }
    m_sounds[uid] = snd;
    return !fail;
}

void CSndSDL::replace(unsigned char *data, unsigned int size, unsigned int uid)
{
    // TODO: get rid of replace
    remove(uid);
    add(data, size, uid);
}

void CSndSDL::remove(unsigned int uid)
{
    if (m_sounds.find(uid) == m_sounds.end())
    {
        printf("REMOVE: sound not found: %u\n", uid);
        return;
    }
    SND *snd = m_sounds[uid];
    for (int i = 0; i < MAX_INSTANCES; ++i)
    {
        if (snd->channels[i] != -1)
        {
            Mix_HaltChannel(snd->channels[i]);
        }
    }
    Mix_FreeChunk(snd->chunk);
    delete snd;
    m_sounds.erase(uid);
}

void CSndSDL::play(unsigned int uid)
{
    SND *snd = m_sounds[uid];
    for (int i = 0; i < MAX_INSTANCES; ++i)
    {
        if (snd->channels[i] != -1 && Mix_Playing(snd->channels[i]))
        {
            // already playing
            continue;
        }
        else
        {
            snd->channels[i] = Mix_PlayChannel(
                snd->channels[i], snd->chunk, 0);
            if (snd->channels[i] == -1)
            {
                printf("Mix_PlayChannel: %s\n", Mix_GetError());
            }
            break;
        }
    }
}

void CSndSDL::stop(unsigned int uid)
{
    SND *snd = m_sounds[uid];
    for (int i = 0; i < MAX_INSTANCES; ++i)
    {
        if (snd->channels[i] != -1)
        {
            Mix_HaltChannel(snd->channels[i]);
            snd->channels[i] = -1;
        }
    }
}

void CSndSDL::stopAll()
{
    Mix_HaltChannel(-1);
}

bool CSndSDL::isValid()
{
    return m_valid;
}

bool CSndSDL::has_sound(unsigned int uid)
{
    return m_sounds.find(uid) != m_sounds.end();
}

const char *CSndSDL::signature() const
{
    return "lgck-sdl-sound";
}

/**
 * @brief Set volume level (0-128)
 *
 * @param v
 */
void CSndSDL::setVolume(int v)
{
    m_volume = v;
    for (auto &[key, val] : m_sounds)
    {
        if (val && val->chunk)
            Mix_VolumeChunk(val->chunk, m_volume);
    }
}

/**
 * @brief Get volume level (0-128)
 *
 * @return int
 */
int CSndSDL::volume() const
{
    return m_volume;
}