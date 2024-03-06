/*
    cs3-runtime-sdl
    Copyright (C) 2024  Francois Blanchette

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
#include "animator.h"
#include "tilesdata.h"
#include "animzdata.h"
#include <cstring>

CAnimator::animzSeq_t CAnimator::m_animzSeq[] = {
    {TILES_DIAMOND, ANIMZ_DIAMOND, 13},
    {TILES_INSECT1, ANIMZ_INSECT1_DN, 8},
    {TILES_SWAMP, ANIMZ_SWAMP, 2},
    {TILES_ALPHA, ANIMZ_ALPHA, 2},
    {TILES_FORCEF94, ANIMZ_FORCEF94, 8},
    {TILES_VAMPLANT, ANIMZ_VAMPLANT, 2},
    {TILES_ORB, ANIMZ_ORB, 4},
    {TILES_TEDDY93, ANIMZ_TEDDY93, 2},
    {TILES_LUTIN, ANIMZ_LUTIN, 2},
    {TILES_OCTOPUS, ANIMZ_OCTOPUS, 2},
    {TILES_TRIFORCE, ANIMZ_TRIFORCE, 4},
    {TILES_YAHOO, ANIMZ_YAHOO, 2},
    {TILES_YIGA, ANIMZ_YIGA, 2},
    {TILES_YELKILLER, ANIMZ_YELKILLER, 2},
    {TILES_MANKA, ANIMZ_MANKA, 2},
};

CAnimator::CAnimator()
{
    const uint32_t seqCount = sizeof(m_animzSeq) / sizeof(animzSeq_t);
    memset(m_tileReplacement, NO_ANIMZ, sizeof(m_tileReplacement));
    m_seqIndex = new int32_t[seqCount];
    memset(m_seqIndex, 0, seqCount * sizeof(uint32_t));
}

CAnimator::~CAnimator()
{
    delete[] m_seqIndex;
}

void CAnimator::animate()
{
    const uint32_t seqCount = sizeof(m_animzSeq) / sizeof(animzSeq_t);
    for (uint32_t i = 0; i < seqCount; ++i)
    {
        const animzSeq_t &seq = m_animzSeq[i];
        int32_t &index = m_seqIndex[i];
        m_tileReplacement[seq.srcTile] = seq.startSeq + index;
        index = index < seq.count - 1 ? index + 1 : 0;
    }
    ++m_offset;
}

uint8_t CAnimator::at(uint8_t tileID)
{
    return m_tileReplacement[tileID];
}

int CAnimator::offset()
{
    return m_offset;
}

bool CAnimator::isSpecialCase(uint8_t tileID)
{
    return tileID == TILES_INSECT1;
}
