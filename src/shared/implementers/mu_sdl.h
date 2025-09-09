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
#pragma once

#include "../interfaces/IMusic.h"
#include <string>
#include <cinttypes>
#include "SDL3/SDL.h"
#include "SDL3_mixer/SDL_mixer.h"

class CMusicSDL : public IMusic
{
public:
    CMusicSDL();
    ~CMusicSDL() override;
    bool open(const char *file) override;
    bool play(int loop = -1) override;
    void stop() override;
    void close() override;
    bool isValid() override;
    const char *signature() const override;
    int getVolume() override;
    void setVolume(int volume) override;
    int type() { return m_type; }
    int isPlaying() { return m_playing; }
    enum
    {
        TYPE_NONE,
        TYPE_OGG,
        TYPE_STREAM
    };

protected:
    typedef struct
    {
        SDL_AudioDeviceID audioDevice;
        Mix_Music *mixData;
        uint8_t *xmData;
    } MusicData;

    MusicData m_data; // Background Music
    char *m_name;
    std::string m_filepath;
    uint8_t m_type = TYPE_NONE;
    bool m_playing = false;
};