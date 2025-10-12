#pragma once

#include "../../../src/shared/FrameSet.h"

namespace SpriteSheet
{
    enum Flag
    {
        noflag = 0,
        sorted = 1,
        no_random_colors = 2,
    };
}

bool toSpriteSheet(CFrameSet &set, std::vector<uint8_t> &png, const SpriteSheet::Flag flags);