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

#pragma once

#include "../interfaces/ISound.h"
#include <unordered_map>
class ISound;

struct Mix_Chunk;
struct SDL_RWops;

class CSndSDL : public ISound
{
public:
    CSndSDL();
    ~CSndSDL() override;
    void forget() override;
    bool add(unsigned char *data, unsigned int size, unsigned int uid) override;
    void replace(unsigned char *data, unsigned int size, unsigned int uid) override;
    void remove(unsigned int uid) override;
    void play(unsigned int uid) override;
    void stop(unsigned int uid) override;
    void stopAll() override;
    bool isValid() override;
    bool has_sound(unsigned int uid) override;
    const char *signature() const override;

    enum
    {
        MAX_INSTANCES = 3
    };
    using SND = struct
    {
        Mix_Chunk *chunk;
        int channels[MAX_INSTANCES];
    };

protected:
    bool m_valid;
};
