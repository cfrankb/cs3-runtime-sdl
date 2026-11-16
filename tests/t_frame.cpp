/*
    cs3-runtime-sdl
    Copyright (C) 2025 Francois Blanchette

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

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "../src/logger.h"
#include "../src/shared/DotArray.h"
#include "../src/shared/FileWrap.h"
#include "../src/shared/Frame.h"
#include "../src/shared/FrameSet.h"
#include "t_frame.h"

namespace
{
// unique non-zero opaque value per pixel
void seedUnique(CFrame &f)
{
    for (int y = 0; y < f.height(); ++y)
    {
        for (int x = 0; x < f.width(); ++x)
        {
            f.at(x, y) = 0xff000000 | (uint32_t)(1 + x + y * f.width());
        }
    }
}

bool checkDims(const CFrame &f, int w, int h, const char *what)
{
    if (f.width() != w || f.height() != h)
    {
        LOGE("%s: dims %dx%d; expecting %dx%d", what, f.width(), f.height(), w, h);
        return false;
    }
    return true;
}
} // namespace

bool test_frame_transforms()
{
    CFrame f(4, 3);
    seedUnique(f);
    const uint32_t v[4][3] = {
        {f.at(0, 0), f.at(0, 1), f.at(0, 2)},
        {f.at(1, 0), f.at(1, 1), f.at(1, 2)},
        {f.at(2, 0), f.at(2, 1), f.at(2, 2)},
        {f.at(3, 0), f.at(3, 1), f.at(3, 2)},
    };

    {
        CFrame t(f);
        t.flipH();
        if (t.at(0, 1) != v[3][1] || t.at(3, 2) != v[0][2])
        {
            LOGE("flipH mapping wrong");
            return false;
        }
    }

    {
        CFrame t(f);
        t.flipV();
        if (t.at(2, 0) != v[2][2] || t.at(2, 2) != v[2][0])
        {
            LOGE("flipV mapping wrong");
            return false;
        }
    }

    {
        CFrame t(f);
        t.rotate();
        if (!checkDims(t, 3, 4, "rotate"))
        {
            return false;
        }
        for (int y = 0; y < 3; ++y)
        {
            for (int x = 0; x < 4; ++x)
            {
                if (t.at(3 - y - 1, x) != f.at(x, y))
                {
                    LOGE("rotate mapping wrong at (%d,%d)", x, y);
                    return false;
                }
            }
        }
    }

    {
        CFrame t(f);
        t.enlarge();
        if (!checkDims(t, 8, 6, "enlarge"))
        {
            return false;
        }
        if (t.at(4, 2) != v[2][1] || t.at(5, 3) != v[2][1])
        {
            LOGE("enlarge 2x2 block not replicated");
            return false;
        }
        t.shrink();
        if (!checkDims(t, 4, 3, "shrink after enlarge"))
        {
            return false;
        }
        for (int y = 0; y < 3; ++y)
        {
            for (int x = 0; x < 4; ++x)
            {
                if (t.at(x, y) != v[x][y])
                {
                    LOGE("shrink(enlarge(f)) differs at (%d,%d)", x, y);
                    return false;
                }
            }
        }
    }

    // shift UP/DOWN over 3 rows labelled 1, 2, 3 (plain values, alpha unused)
    auto labelledRows = [](int n)
    {
        CFrame g(4, 3);
        for (int y = 0; y < 3; ++y)
        {
            for (int x = 0; x < 4; ++x)
            {
                g.at(x, y) = static_cast<uint32_t>(y + 1);
            }
        }
        (void)n;
        return g;
    };

    {
        CFrame s = labelledRows(0);
        s.shiftUP(true);
        if (s.at(0, 0) != 2 || s.at(0, 2) != 1)
        {
            LOGE("shiftUP wrap wrong (top did not reappear at bottom)");
            return false;
        }
    }
    {
        CFrame s = labelledRows(0);
        s.shiftUP(false);
        if (s.at(0, 0) != 2 || s.at(0, 2) != 0)
        {
            LOGE("shiftUP clip wrong (bottom not cleared)");
            return false;
        }
    }

    {
        CFrame s = labelledRows(0);
        s.shiftDOWN(true);
        if (s.at(0, 0) != 3 || s.at(0, 1) != 1)
        {
            LOGE("shiftDOWN wrap wrong");
            return false;
        }
    }
    {
        CFrame s = labelledRows(0);
        s.shiftDOWN(false);
        if (s.at(0, 0) != 0 || s.at(0, 2) != 2)
        {
            LOGE("shiftDOWN clip wrong");
            return false;
        }
    }

    CFrame w(4, 3);
    for (int y = 0; y < 3; ++y)
    {
        for (int x = 0; x < 4; ++x)
        {
            w.at(x, y) = static_cast<uint32_t>(x + 1);
        }
    }
    {
        CFrame s(w);
        s.shiftLEFT(true);
        if (s.at(0, 1) != 2 || s.at(3, 1) != 1)
        {
            LOGE("shiftLEFT wrap column mapping wrong");
            return false;
        }
    }
    {
        CFrame s(w);
        s.shiftLEFT(false);
        if (s.at(3, 1) != 0)
        {
            LOGE("shiftLEFT clip did not clear last column");
            return false;
        }
    }
    {
        CFrame s(w);
        s.shiftRIGHT(true);
        if (s.at(0, 1) != 4 || s.at(3, 1) != 3)
        {
            LOGE("shiftRIGHT wrap column mapping wrong");
            return false;
        }
    }
    {
        CFrame s(w);
        s.shiftRIGHT(false);
        if (s.at(0, 1) != 0 || s.at(1, 1) != 1)
        {
            LOGE("shiftRIGHT clip wrong");
            return false;
        }
    }
    return true;
}

bool test_frame_fills()
{
    CFrame f(4, 2);
    if (!f.isEmpty())
    {
        LOGE("fresh frame not empty");
        return false;
    }
    f.fill(0x00112233);
    if (!f.isEmpty())
    {
        LOGE("fill with alpha 0 is not empty");
        return false;
    }
    f.fill(0xff112233);
    if (f.isEmpty() || f.at(3, 1) != 0xff112233)
    {
        LOGE("fill(0xff112233) mismatch");
        return false;
    }

    CFrame g = f;
    g.clear();
    if (g.width() != 0 || g.height() != 0 || !g.isEmpty())
    {
        LOGE("clear() left dimensions");
        return false;
    }
    g.copy(&f);
    if (g.width() != 4 || g.at(0, 0) != 0xff112233)
    {
        LOGE("copy() mismatch");
        return false;
    }
    g.copy(nullptr);
    if (g.width() != 0)
    {
        LOGE("copy(nullptr) did not clear");
        return false;
    }

    CFrame h(1, 1);
    h.fill(0xff123456);
    h.inverse();
    if (h.at(0, 0) != 0xffedcba9)
    {
        LOGE("inverse gave %08x; expecting ffeedcba9", h.at(0, 0));
        return false;
    }
    h.fill(0x80112233);
    h.shadow(2);
    if (h.at(0, 0) != 0x40112233)
    {
        LOGE("shadow(2) gave %08x; expecting 40112233", h.at(0, 0));
        return false;
    }
    h.fill(0xff112233);
    h.fade(128);
    if (h.at(0, 0) != 0x80112233)
    {
        LOGE("fade(128) gave %08x; expecting 80112233", h.at(0, 0));
        return false;
    }

    h.fill(0xff112233);
    h.abgr2argb();
    if (h.at(0, 0) != 0xff332211)
    {
        LOGE("abgr2argb gave %08x; expecting ff332211", h.at(0, 0));
        return false;
    }
    h.fill(0x00112233); // no alpha: red/blue bytes are dropped, not swapped
    h.abgr2argb();
    if (h.at(0, 0) != 0x00002200)
    {
        LOGE("abgr2argb(alpha=0) gave %08x; expecting 00002200", h.at(0, 0));
        return false;
    }
    h.fill(0xff112233);
    h.argb2arbg();
    if (h.at(0, 0) != 0xff221133)
    {
        LOGE("argb2arbg gave %08x; expecting ff221133", h.at(0, 0));
        return false;
    }
    return true;
}

bool test_frame_geometry()
{
    // resize keeps the overlap, zeroes the new area
    CFrame f(4, 4);
    f.fill(0xff123456);
    f.resize(6, 2);
    if (!checkDims(f, 6, 2, "resize"))
    {
        return false;
    }
    if (f.at(0, 0) != 0xff123456 || f.at(5, 1) != 0)
    {
        LOGE("resize did not zero the new area");
        return false;
    }

    // clip subgrid
    CFrame s(4, 4);
    seedUnique(s);
    CFrame *sub = s.clip(1, 1, 2, 2);
    if (!sub || sub->at(1, 1) != s.at(2, 2) || sub->at(0, 0) != s.at(1, 1))
    {
        LOGE("clip(1,1,2,2) subgrid wrong");
        delete sub;
        return false;
    }
    delete sub;
    sub = s.clip(2, 2, -1, -1);
    if (!checkDims(*sub, 2, 2, "clip(-1,-1)"))
    {
        delete sub;
        return false;
    }
    delete sub;
    sub = s.clip(0, 0, 0, 3); // cx < 1 forces a 0x0 frame
    if (!checkDims(*sub, 0, 0, "clip(0-width)"))
    {
        delete sub;
        return false;
    }
    delete sub;
    bool threw = false;
    try
    {
        sub = s.clip(1, 1, 10, 2); // unclamped extent: throws (finding)
        delete sub;
    }
    catch (const std::out_of_range &)
    {
        threw = true;
    }
    if (!threw)
    {
        LOGE("out-of-bounds clip did not throw");
        return false;
    }

    // drawAt with and without transparency
    CFrame base(4, 4);
    base.fill(0x01);
    CFrame ov(2, 2);
    ov.at(0, 0) = 0; // transparent
    ov.at(1, 0) = 0xff000001;
    ov.at(0, 1) = 0xff000002;
    ov.at(1, 1) = 0xff000003;
    base.drawAt(ov, 1, 1, true);
    if (base.at(1, 1) != 0x01 || base.at(2, 1) != 0xff000001 ||
        base.at(1, 2) != 0xff000002)
    {
        LOGE("drawAt transparency mode wrong");
        return false;
    }
    base.drawAt(ov, 1, 1, false);
    if (base.at(1, 1) != 0)
    {
        LOGE("drawAt opaque mode skipped the zero pixel");
        return false;
    }

    // save + draw (the paint-path pair), pen size 2
    CFrame p(4, 4);
    p.fill(0x01);
    const std::vector<Dot> dots = {
        Dot{2, 2, 0xff112233},
        Dot{9, 9, 0xff445566}, // fully out of bounds: ignored
    };
    std::vector<Dot> saved;
    p.save(dots, saved, 2);
    if (saved.size() != 4 || saved[0].x != 2 || saved[0].y != 2 ||
        saved[0].color != 0x01)
    {
        LOGE("save() captured %zu dots", saved.size());
        return false;
    }
    if (!p.draw(dots, 2))
    {
        LOGE("draw() reported no change");
        return false;
    }
    if (p.at(2, 2) != 0xff112233 || p.at(3, 3) != 0xff112233 || p.at(0, 0) != 0x01)
    {
        LOGE("draw() pen block wrong");
        return false;
    }
    if (p.draw(dots, 2))
    {
        LOGE("redraw reported a change");
        return false;
    }

    // paint modes: COLOR_ONLY (1) keeps alpha, ALPHA_ONLY (2) keeps colour;
    // the MODE_* enum is private, the values are the wire contract
    CFrame m(2, 2);
    m.fill(0x40000000);
    const std::vector<Dot> one = {Dot(0, 0, 0xff112233)};
    if (!m.draw(one, 1, 1) || m.at(0, 0) != 0x40112233)
    {
        LOGE("MODE_COLOR_ONLY gave %08x; expecting 40112233", m.at(0, 0));
        return false;
    }
    m.fill(0x01020304);
    const std::vector<Dot> alphaDot = {Dot(0, 0, 0x90112233)};
    if (!m.draw(alphaDot, 1, 2) || m.at(0, 0) != 0x90020304)
    {
        LOGE("MODE_ALPHA_ONLY gave %08x; expecting 90020304", m.at(0, 0));
        return false;
    }
    return true;
}

bool test_frame_pixels()
{
    CFrame f(4, 4);
    if (!f.isValid(0, 0) || !f.isValid(3, 3) || f.isValid(4, 0) || f.isValid(0, 4) ||
        f.isValid(-1, 0))
    {
        LOGE("isValid bounds wrong");
        return false;
    }
    if (CFrame::toNet(0x11223344u) != 0x44332211u)
    {
        LOGE("toNet did not reverse bytes");
        return false;
    }
    if (std::string(CFrame::getChunkType()) != "obLT")
    {
        LOGE("getChunkType mismatch");
        return false;
    }

    // transparency: matched colour goes fully transparent, others untouched
    f.fill(0xff112233);
    f.at(0, 0) = 0xff998877;
    if (f.hasTransparency())
    {
        LOGE("opaque frame reports transparency");
        return false;
    }
    f.setTransparency(0x112233);
    if (!f.hasTransparency() || f.at(0, 0) != 0xff998877 || f.at(3, 3) != 0)
    {
        LOGE("setTransparency mismatch");
        return false;
    }
    f.fill(0xff112233);
    f.setTopPixelAsTranparency();
    if (f.at(0, 0) != 0 || f.at(3, 3) != 0)
    {
        LOGE("setTopPixelAsTranparency did not clear matches");
        return false;
    }

    // bounded flood fill: region fills, border and beyond stay put
    f.fill(2);
    f.at(0, 0) = 1;
    f.at(1, 0) = 1;
    f.at(0, 1) = 1;
    f.at(1, 1) = 1;
    f.floodFill(0, 0, 1, 3);
    if (f.at(0, 0) != 3 || f.at(1, 1) != 3 || f.at(2, 0) != 2 || f.at(0, 2) != 2)
    {
        LOGE("floodFill leaked or under-filled");
        return false;
    }
    // diagonal-only neighbour is not filled (scanline algorithm property)
    CFrame d(3, 3);
    d.fill(0);
    d.at(0, 0) = 1;
    d.at(1, 1) = 1;
    d.at(2, 2) = 1;
    d.floodFill(0, 0, 1, 5);
    if (d.at(0, 0) != 5 || d.at(1, 1) != 1 || d.at(2, 2) != 1)
    {
        LOGE("floodFill crossed a diagonal");
        return false;
    }

    // floodFillAlpha keys on alpha only, colours preserved
    CFrame a(4, 4);
    a.fill(0xff112233);
    a.at(0, 0) = 0x50112233;
    a.at(1, 0) = 0x50112233;
    a.at(0, 1) = 0x50112233;
    a.floodFillAlpha(0, 0, 0x50, 0x90);
    if (a.at(1, 1) != 0xff112233 || (a.at(0, 0) & 0xff000000) != 0x90000000 ||
        (a.at(0, 1) & 0x00ffffff) != 0x00112233)
    {
        LOGE("floodFillAlpha mismatch");
        return false;
    }

    // split: 8x8 into 4 tiles; py=0 means full height
    CFrame big(8, 8);
    seedUnique(big);
    std::unique_ptr<CFrameSet> tiles(big.split(4, 4));
    if (!tiles || tiles->size() != 4)
    {
        LOGE("split(4,4) produced %zu tiles", tiles ? tiles->size() : 0);
        return false;
    }
    if (!checkDims(*(*tiles)[3], 4, 4, "split tile"))
    {
        return false;
    }
    tiles.reset(big.split(4, 0));
    if (!tiles || tiles->size() != 2 || (*tiles)[0]->height() != 8)
    {
        LOGE("split(4,0) mismatch");
        return false;
    }
    tiles.reset(big.split(0, 4));
    if (tiles)
    {
        LOGE("split(0,4) should fail");
        return false;
    }

    // explode by column widths
    uint16_t sx[2] = {4, 4};
    uint16_t sy[2] = {8, 8};
    std::unique_ptr<CFrameSet> cols(big.explode(2, sx, sy));
    if (!cols || cols->size() != 2 || (*cols)[1]->width() != 4 ||
        (*cols)[1]->at(0, 0) != big.at(4, 0))
    {
        LOGE("explode(sx,sy) mismatch");
        return false;
    }

    // explode by oblv2 metadata: INVALID position yields a blank frame
    std::vector<CFrame::oblv2DataUnit_t> meta = {
        {0, 0, 4, 4},
        {0xffff, 0xffff, 2, 2},
    };
    std::unique_ptr<CFrameSet> units(big.explode(meta));
    if (!units || units->size() != 2 || (*units)[0]->at(0, 0) != big.at(0, 0) ||
        !(*units)[1]->isEmpty())
    {
        LOGE("explode(metadata) mismatch");
        return false;
    }
    return true;
}

bool test_frame_io()
{
    CFrame f(4, 4);
    seedUnique(f);

    constexpr const char *PATH = "tests/out/frame_io.bin";
    {
        CFileWrap fw;
        if (!fw.open(PATH, "wb"))
        {
            LOGE("frame io: cannot create %s", PATH);
            return false;
        }
        if (!f.write(fw))
        {
            LOGE("frame io: write failed [%s]", f.getLastError());
            return false;
        }
        fw.close();
    }
    {
        CFileWrap fw;
        if (!fw.open(PATH, "rb"))
        {
            LOGE("frame io: cannot reopen %s", PATH);
            return false;
        }
        CFrame back;
        if (!back.read(fw))
        {
            LOGE("frame io: read failed [%s]", back.getLastError());
            return false;
        }
        if (!checkDims(back, 4, 4, "frame round-trip"))
        {
            return false;
        }
        for (int y = 0; y < 4; ++y)
        {
            for (int x = 0; x < 4; ++x)
            {
                if (back.at(x, y) != f.at(x, y))
                {
                    LOGE("frame round-trip pixel (%d,%d) differs", x, y);
                    return false;
                }
            }
        }
        fw.close();
    }

    // toBmp: buffer + 'BM' magic + size field consistency
    uint8_t *bmp = nullptr;
    int bmpSize = 0;
    f.toBmp(bmp, bmpSize);
    const int pitch = (4 * 3 + 3) & ~3;
    const int expect = 54 + pitch * 4;
    if (!bmp || bmpSize != expect || bmp[0] != 'B' || bmp[1] != 'M')
    {
        LOGE("toBmp: size %d magic %c%c; expecting %d 'BM'", bmpSize,
             bmp ? bmp[0] : '?', bmp ? bmp[1] : '?', expect);
        delete[] bmp;
        return false;
    }
    delete[] bmp;

    // toPng: signature, IHDR dimensions, IEND terminator
    std::vector<uint8_t> png;
    if (!f.toPng(png) || png.size() < 30)
    {
        LOGE("toPng produced %zu bytes", png.size());
        return false;
    }
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (memcmp(png.data(), sig, 8) != 0)
    {
        LOGE("toPng: bad signature");
        return false;
    }
    if (png[12] != 'I' || png[13] != 'H' || png[14] != 'D' || png[15] != 'R' ||
        png[16] != 0 || png[17] != 0 || png[18] != 0 || png[19] != 4 ||
        png[20] != 0 || png[21] != 0 || png[22] != 0 || png[23] != 4)
    {
        LOGE("toPng: bad IHDR");
        return false;
    }
    if (png[png.size() - 8] != 'I' || png[png.size() - 7] != 'E' ||
        png[png.size() - 6] != 'N' || png[png.size() - 5] != 'D')
    {
        LOGE("toPng: missing IEND");
        return false;
    }
    return true;
}
