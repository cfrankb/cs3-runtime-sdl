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
#ifndef MU_SDL_H
#define MU_SDL_H

#include "../interfaces/IMusic.h"
#include "SDL2/SDL_mixer.h"

class CMusicSDL : public IMusic
{
public:
    CMusicSDL();
    virtual ~CMusicSDL();
    virtual bool open(const char *file);
    virtual bool play(int loop = -1);
    virtual void stop();
    virtual void close();
    virtual bool isValid();
    virtual const char *signature() const;

protected:
    Mix_Music *m_music; // Background Music
    char *m_name;
};

#endif // MU_SDL_H
