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

#include <cstring>
#include <cstdlib>
#include <zlib.h>
#include "../src/shared/FileWrap.h"
#include "../src/shared/helper.h"
#include "../src/logger.h"
#include "../src/gamemixin.h"
#include "../src/skills.h"
#include "../src/runtime.h"
#include "../src/game.h"
#include "../src/shared/PngMagic.h"
#include "../src/shared/FrameSet.h"
#include "thelper.h"
#include "t_runtime.h"
#include <cstdio>
#include <filesystem>

constexpr const char *INPUT_FILE = "tests/in/annie.obl";
constexpr const char *OUTPUT_FILE = "tests/out/annie.obl";

bool test_png_magic()
{
    CFrameSet fs;
    CFileWrap file;
    if (!file.open(INPUT_FILE, "rb"))
    {
        LOGE("can't open for reading %s", INPUT_FILE);
        return false;
    }

    if (!parsePNG(fs, file))
    {
        file.close();
        LOGE("can't decode png: %s; last_error: %s", INPUT_FILE, fs.getLastError());
        return false;
    }

    file.close();

    std::vector<uint8_t> png;
    if (!fs.toPng(png))
    {
        LOGE("cannot convert to png");
        return false;
    }

    if (!file.open(OUTPUT_FILE, "wb"))
    {
        LOGE("can't open for writing %s", OUTPUT_FILE);
        return false;
    }

    if (file.write(png.data(), png.size()) != 1)
    {
        LOGE("failed to write file: %s", OUTPUT_FILE);
        return false;
    }

    file.close();

    if (!compareFiles(INPUT_FILE, OUTPUT_FILE))
    {
        return false;
    }

    // clean up
    std::filesystem::remove(OUTPUT_FILE);

    return true;
}

namespace
{
constexpr const uint8_t PNG_SIG[8] = {137, 80, 78, 71, 13, 10, 26, 10};

// write raw bytes to a scratch file and reopen for reading
bool writeScratch(const char *path, const std::vector<uint8_t> &data)
{
    FILE *f = fopen(path, "wb");
    if (!f)
    {
        LOGE("png tests: cannot create %s", path);
        return false;
    }
    const size_t n = fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    return n == data.size();
}

bool parseFile(CFrameSet &fs, const std::string &path, int orgPos = 0,
               bool skipOblt = false)
{
    CFileWrap file;
    if (!file.open(path.c_str(), "rb"))
    {
        LOGE("png tests: cannot open %s", path.c_str());
        return false;
    }
    const bool ok = parsePNG(fs, file, orgPos, skipOblt);
    file.close();
    return ok;
}

void addSyntheticFrames(CFrameSet &fs)
{
    CFrame *opaque = new CFrame(8, 8);
    opaque->fill(0xff112233);
    CFrame *transp = new CFrame(8, 8);
    transp->fill(0xff445566);
    transp->setTransparency(0x445566); // whole frame -> alpha 0
    fs.add(opaque);
    fs.add(transp);
    fs.setName("synth");
}
} // namespace

bool test_png_refs()
{
    for (const auto &entry : std::filesystem::directory_iterator("tests/in/refs"))
    {
        if (entry.path().extension() != ".png")
        {
            continue;
        }
        const std::string path = entry.path().string();

        CFrameSet full;
        if (!parseFile(full, path) || full.size() == 0)
        {
            LOGE("refs: parse failed %s [%s]", path.c_str(), full.getLastError());
            return false;
        }
        CFrameSet whole;
        if (!parseFile(whole, path, 0, true) || whole.size() != 1)
        {
            LOGE("refs: skipOblt did not yield one whole frame: %s (%zu)",
                 path.c_str(), whole.size());
            return false;
        }
        if (full.size() < whole.size())
        {
            LOGE("refs: %s obLT split (%zu) smaller than whole (%zu)", path.c_str(),
                 full.size(), whole.size());
            return false;
        }
        CFrame *f0 = full[0];
        if (!f0 || f0->width() <= 0 || f0->height() <= 0 || f0->isEmpty())
        {
            LOGE("refs: %s first frame is blank/invalid", path.c_str());
            return false;
        }
        LOGI("refs: %s -> %zu frames, first %dx%d", path.c_str(), full.size(),
             f0->width(), f0->height());
    }
    return true;
}

bool test_png_roundtrip()
{
    CFrameSet fs;
    addSyntheticFrames(fs);
    std::vector<uint8_t> png;
    if (!fs.toPng(png))
    {
        LOGE("roundtrip: synthetic toPng failed [%s]", fs.getLastError());
        return false;
    }
    constexpr const char *PATH = "tests/out/synth.png";
    if (!writeScratch(PATH, png))
    {
        return false;
    }
    CFrameSet back;
    if (!parseFile(back, PATH) || back.size() != fs.size())
    {
        LOGE("roundtrip: parsed %zu frames; expecting %zu [%s]", back.size(),
             fs.size(), back.getLastError());
        return false;
    }
    for (size_t i = 0; i < fs.size(); ++i)
    {
        CFrame *src = fs[i];
        CFrame *dst = back[i];
        if (dst->width() != src->width() || dst->height() != src->height())
        {
            LOGE("roundtrip: frame %zu dims %dx%d; expecting %dx%d", i,
                 dst->width(), dst->height(), src->width(), src->height());
            return false;
        }
        const uint32_t s = src->at(0, 0);
        const uint32_t d = dst->at(0, 0);
        if ((s & 0xffffff) != (d & 0xffffff))
        {
            LOGE("roundtrip: frame %zu colour %06x; expecting %06x", i,
                 d & 0xffffff, s & 0xffffff);
            return false;
        }
        const uint8_t sa = static_cast<uint8_t>(s >> 24);
        const uint8_t da = static_cast<uint8_t>(d >> 24);
        if ((sa == 0) != (da == 0))
        {
            LOGE("roundtrip: frame %zu transparency flipped (alpha %02x)", i, da);
            return false;
        }
    }
    std::filesystem::remove(PATH);
    return true;
}

bool test_png_corrupt()
{
    CFrameSet fs;
    addSyntheticFrames(fs);
    std::vector<uint8_t> png;
    if (!fs.toPng(png))
    {
        LOGE("corrupt: synthetic toPng failed");
        return false;
    }
    CFrameSet sanity;
    if (!writeScratch("tests/out/png_ok.png", png) ||
        !parseFile(sanity, "tests/out/png_ok.png"))
    {
        LOGE("corrupt: unmodified stream does not parse");
        return false;
    }

    // bad signature
    std::vector<uint8_t> sig(png);
    sig[1] ^= 0xff;
    constexpr const char *SIGPATH = "tests/out/png_badsig.png";
    if (writeScratch(SIGPATH, sig))
    {
        CFrameSet bad;
        if (parseFile(bad, SIGPATH) || bad.getLastError()[0] == '\0')
        {
            LOGE("corrupt: bad signature accepted");
            return false;
        }
        std::filesystem::remove(SIGPATH);
    }
    else
    {
        return false;
    }

    // single-byte corruption of the first chunk's CRC (first exercise of the
// zlib crc32 chunk check)
    const int ihdrLen = (png[8] << 24) | (png[9] << 16) | (png[10] << 8) | png[11];
    std::vector<uint8_t> crc(png);
    crc[8 + 4 + 4 + ihdrLen] ^= 0x01; // first CRC byte of IHDR
    constexpr const char *CRC_PATH = "tests/out/png_badcrc.png";
    if (!writeScratch(CRC_PATH, crc))
    {
        return false;
    }
    CFrameSet badCrc;
    if (parseFile(badCrc, CRC_PATH) ||
        std::string(badCrc.getLastError()).find("CRC") == std::string::npos)
    {
        LOGE("corrupt: bad CRC accepted [%s]", badCrc.getLastError());
        return false;
    }
    std::filesystem::remove(CRC_PATH);

    // truncated stream
    std::vector<uint8_t> trunc(png.begin(), png.begin() + png.size() / 2);
    constexpr const char *TR_PATH = "tests/out/png_trunc.png";
    if (!writeScratch(TR_PATH, trunc))
    {
        return false;
    }
    CFrameSet badTrunc;
    if (parseFile(badTrunc, TR_PATH) || badTrunc.getLastError()[0] == '\0')
    {
        LOGE("corrupt: truncated stream accepted");
        return false;
    }
    std::filesystem::remove(TR_PATH);
    std::filesystem::remove("tests/out/png_ok.png");
    return true;
}

bool test_png_orgpos()
{
    CFrameSet fs;
    addSyntheticFrames(fs);
    std::vector<uint8_t> png;
    if (!fs.toPng(png))
    {
        LOGE("orgpos: synthetic toPng failed");
        return false;
    }
    const uint8_t junk[7] = {1, 2, 3, 4, 5, 6, 7};
    std::vector<uint8_t> wrapped(junk, junk + sizeof(junk));
    wrapped.insert(wrapped.end(), png.begin(), png.end());
    constexpr const char *PATH = "tests/out/png_wrapped.png";
    if (!writeScratch(PATH, wrapped))
    {
        return false;
    }
    CFrameSet atZero;
    if (parseFile(atZero, PATH)) // default orgPos 0: signature mismatch
    {
        LOGE("orgpos: junk header parsed at orgPos 0");
        return false;
    }
    CFrameSet atOffset;
    if (!parseFile(atOffset, PATH, 7) || atOffset.size() == 0)
    {
        LOGE("orgpos: parse at offset 7 failed [%s]", atOffset.getLastError());
        return false;
    }
    std::filesystem::remove(PATH);
    return true;
}
namespace
{
void pushBE(std::vector<uint8_t> &v, uint32_t n)
{
    v.push_back(static_cast<uint8_t>(n >> 24));
    v.push_back(static_cast<uint8_t>(n >> 16));
    v.push_back(static_cast<uint8_t>(n >> 8));
    v.push_back(static_cast<uint8_t>(n));
}

void appendChunk(std::vector<uint8_t> &png, const char type[4],
                 const std::vector<uint8_t> &data)
{
    pushBE(png, static_cast<uint32_t>(data.size()));
    const size_t crcStart = png.size();
    png.insert(png.end(), type, type + 4);
    png.insert(png.end(), data.begin(), data.end());
    pushBE(png, static_cast<uint32_t>(
                    crc32(0, png.data() + crcStart,
                          static_cast<unsigned>(png.size() - crcStart))));
}

// 8x8 4-bit indexed PNG: column 0 uses palette index 0 (transparent via tRNS),
// the rest index 1; plus an obLT version-2 chunk splitting it into two 4x8 frames
std::vector<uint8_t> build4bppPng(bool withObltV2)
{
    std::vector<uint8_t> png(std::begin(PNG_SIG), std::end(PNG_SIG));

    std::vector<uint8_t> ihdr;
    pushBE(ihdr, 8); // width
    pushBE(ihdr, 8); // height
    ihdr.push_back(4);  // bit depth
    ihdr.push_back(3);  // color type: palette
    ihdr.push_back(0);  // compression
    ihdr.push_back(0);  // filter
    ihdr.push_back(0);  // interlace
    appendChunk(png, "IHDR", ihdr);

    // palette entry i stored so that memcpy gives 0x00bbggrr:
    // index 0 -> 0x00112233, index 1 -> 0x00445566
    std::vector<uint8_t> plte(16 * 3, 0);
    plte[0] = 0x33; plte[1] = 0x22; plte[2] = 0x11;
    plte[3] = 0x66; plte[4] = 0x55; plte[5] = 0x44;
    appendChunk(png, "PLTE", plte);

    std::vector<uint8_t> trns(16, 0xff);
    trns[0] = 0; // index 0 fully transparent
    appendChunk(png, "tRNS", trns);

    std::vector<uint8_t> raw;
    for (int y = 0; y < 8; ++y)
    {
        raw.push_back(0); // filter: none
        for (int b = 0; b < 4; ++b)
        {
            raw.push_back(0x01); // high nibble index 0, low nibble index 1
        }
    }
    std::vector<uint8_t> zipped(compressBound(raw.size()));
    uLongf zlen = zipped.size();
    if (compress2(zipped.data(), &zlen, raw.data(), raw.size(), Z_DEFAULT_COMPRESSION) != Z_OK)
    {
        LOGE("build4bpp: compress2 failed");
        return {};
    }
    zipped.resize(zlen);
    appendChunk(png, "IDAT", zipped);

    if (withObltV2)
    {
        std::vector<uint8_t> oblt;
        // obLT header fields are written by toPng in native byte order
        const uint32_t fields[3] = {0, 2, 2}; // Reserved, Version 2, Count
        for (const uint32_t f : fields)
        {
            for (int b = 0; b < 4; ++b)
            {
                oblt.push_back(static_cast<uint8_t>((f >> (8 * b)) & 0xff));
            }
        }
        const uint16_t units[][4] = {{0, 0, 4, 8}, {4, 0, 4, 8}};
        for (const auto &u : units)
        {
            for (const uint16_t v : u)
            {
                oblt.push_back(static_cast<uint8_t>(v & 0xff));
                oblt.push_back(static_cast<uint8_t>(v >> 8));
            }
        }
        appendChunk(png, "obLT", oblt);
    }

    appendChunk(png, "IEND", {});
    return png;
}
} // namespace

bool test_png_4bpp()
{
    const std::vector<uint8_t> png = build4bppPng(true);
    if (png.empty())
    {
        return false;
    }
    constexpr const char *PATH = "tests/out/indexed4.png";
    if (!writeScratch(PATH, png))
    {
        return false;
    }

    CFrameSet full;
    if (!parseFile(full, PATH))
    {
        LOGE("4bpp: parse failed [%s]", full.getLastError());
        return false;
    }
    if (full.size() != 2)
    {
        LOGE("4bpp: obLT v2 produced %zu frames; expecting 2", full.size());
        return false;
    }
    CFrame *f0 = full[0];
    if (f0->width() != 4 || f0->height() != 8)
    {
        LOGE("4bpp: frame0 dims %dx%d; expecting 4x8", f0->width(), f0->height());
        return false;
    }
    if (f0->at(0, 0) != 0x00112233 || f0->at(1, 0) != 0xff445566 ||
        f0->at(3, 7) != 0xff445566)
    {
        LOGE("4bpp: pixels %08x %08x %08x wrong", f0->at(0, 0), f0->at(1, 0),
             f0->at(3, 7));
        return false;
    }
    CFrame *f1 = full[1];
    if (f1->at(0, 0) != 0x00112233 || f1->at(1, 7) != 0xff445566)
    {
        LOGE("4bpp: frame1 pixels %08x %08x wrong", f1->at(0, 0), f1->at(1, 7));
        return false;
    }

    // skipOblt keeps the whole 8x8 sheet
    CFrameSet whole;
    if (!parseFile(whole, PATH, 0, true) || whole.size() != 1 ||
        whole[0]->width() != 8 || whole[0]->at(0, 0) != 0x00112233)
    {
        LOGE("4bpp: skipOblt whole-sheet mismatch [%s]", whole.getLastError());
        return false;
    }

    // unsupported color type (grayscale) is rejected
    std::vector<uint8_t> gray(std::begin(PNG_SIG), std::end(PNG_SIG));
    std::vector<uint8_t> ihdr;
    pushBE(ihdr, 8);
    pushBE(ihdr, 8);
    ihdr.push_back(8);
    ihdr.push_back(0); // grayscale
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    appendChunk(gray, "IHDR", ihdr);
    std::vector<uint8_t> raw(8 * (1 + 8), 0);
    std::vector<uint8_t> zipped(compressBound(raw.size()));
    uLongf zlen = zipped.size();
    compress2(zipped.data(), &zlen, raw.data(), raw.size(), Z_DEFAULT_COMPRESSION);
    zipped.resize(zlen);
    appendChunk(gray, "IDAT", zipped);
    appendChunk(gray, "IEND", {});
    constexpr const char *GRAY_PATH = "tests/out/gray8.png";
    if (!writeScratch(GRAY_PATH, gray))
    {
        return false;
    }
    CFrameSet bad;
    if (parseFile(bad, GRAY_PATH) ||
        std::string(bad.getLastError()) != "unsupported png")
    {
        LOGE("4bpp: grayscale accepted [%s]", bad.getLastError());
        return false;
    }

    std::filesystem::remove(PATH);
    std::filesystem::remove(GRAY_PATH);
    return true;
}

namespace
{
int paeth(int a, int b, int c)
{
    const int p = a + b - c;
    const int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    if (pa <= pb && pa <= pc)
    {
        return a;
    }
    return pb <= pc ? b : c;
}

// 4-pixel-wide 8-bit PNG (palette or RGBA) whose rows use the given per-row
// PNG filters over a constant image, so exact filter bytes are computable
std::vector<uint8_t> buildFilteredPng(const int colorType, const int filters[],
                                      const int nRows, bool corruptIdat = false,
                                      bool splitIdat = false)
{
    const int pw = colorType == 3 ? 1 : 4;
    const uint8_t K = colorType == 3 ? 1 : 0x10;
    std::vector<uint8_t> png(std::begin(PNG_SIG), std::end(PNG_SIG));

    std::vector<uint8_t> ihdr;
    pushBE(ihdr, 4); // width
    pushBE(ihdr, nRows);
    ihdr.push_back(8);
    ihdr.push_back(static_cast<uint8_t>(colorType));
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);
    appendChunk(png, "IHDR", ihdr);

    if (colorType == 3)
    {
        std::vector<uint8_t> plte(256 * 3, 0);
        plte[3] = 0x66; plte[4] = 0x55; plte[5] = 0x44; // index 1
        appendChunk(png, "PLTE", plte);
    }

    std::vector<uint8_t> raw;
    for (int y = 0; y < nRows; ++y)
    {
        const int f = filters[y];
        raw.push_back(static_cast<uint8_t>(f));
        for (int x = 0; x < 4; ++x)
        {
            int pred = 0;
            switch (f)
            {
            case 0: pred = 0; break;
            case 1: pred = x ? K : 0; break;
            case 2: pred = y ? K : 0; break;
            case 3: pred = ((x ? K : 0) + (y ? K : 0)) / 2; break;
            case 4: pred = paeth(x ? K : 0, y ? K : 0, (x && y) ? K : 0); break;
            default: pred = 0; break;
            }
            for (int b = 0; b < pw; ++b)
            {
                raw.push_back(static_cast<uint8_t>((K - pred) & 0xff));
            }
        }
    }
    std::vector<uint8_t> zipped(compressBound(raw.size()));
    uLongf zlen = zipped.size();
    compress2(zipped.data(), &zlen, raw.data(), raw.size(), Z_DEFAULT_COMPRESSION);
    zipped.resize(zlen);
    if (corruptIdat && zipped.size() > 4)
    {
        zipped[2] ^= 0xff; // valid chunk CRC, broken zlib stream
    }
    if (splitIdat && zipped.size() > 8)
    {
        const size_t half = zipped.size() / 2;
        appendChunk(png, "IDAT", std::vector<uint8_t>(zipped.begin(), zipped.begin() + half));
        appendChunk(png, "IDAT", std::vector<uint8_t>(zipped.begin() + half, zipped.end()));
    }
    else
    {
        appendChunk(png, "IDAT", zipped);
    }
    appendChunk(png, "IEND", {});
    return png;
}

bool parseBytes(CFrameSet &fs, const std::vector<uint8_t> &png, const char *path)
{
    return writeScratch(path, png) && [&]
    {
        const bool ok = parseFile(fs, path);
        std::filesystem::remove(path);
        return ok;
    }();
}

bool checkPixels(CFrameSet &fs, uint32_t expected, int xEnd, int yStart,
                 int yEnd, const char *what)
{
    CFrame *f = fs[0];
    for (int y = yStart; y < yEnd; ++y)
    {
        for (int x = 0; x < xEnd; ++x)
        {
            if (f->at(x, y) != expected)
            {
                LOGE("filters: %s pixel (%d,%d) = %08x; expecting %08x", what, x,
                     y, f->at(x, y), expected);
                return false;
            }
        }
    }
    return true;
}
} // namespace

bool test_png_filters()
{
    enablePngMagicVerbose();
    constexpr const uint32_t RGBA = 0x10101010;

    struct
    {
        int colorType;
        int nRows;
        int filters[5];
        uint32_t expected;
        const char *what;
    } cases[] = {
        {6, 4, {0, 1, 2, 3, 0}, RGBA, "rgba f0-3"},
        {6, 4, {4, 4, 4, 4, 0}, RGBA, "rgba paeth"},
        {3, 5, {0, 1, 2, 3, 4}, 0xff445566, "palette f0-4"},
    };
    for (const auto &c : cases)
    {
        CFrameSet fs;
        const std::vector<uint8_t> png =
            buildFilteredPng(c.colorType, c.filters, c.nRows);
        if (!parseBytes(fs, png, "tests/out/filters.png") || fs.size() != 1)
        {
            LOGE("filters: %s parse failed [%s]", c.what, fs.getLastError());
            return false;
        }
        const int offsetY = (8 - (c.nRows & 7)) & 7;
        if (!checkPixels(fs, c.expected, 4, offsetY, offsetY + c.nRows, c.what))
        {
            return false;
        }
    }

    // multi-IDAT concatenation decodes identically
    {
        const int f[2] = {0, 0};
        CFrameSet fs;
        const std::vector<uint8_t> png =
            buildFilteredPng(6, f, 2, false, true);
        if (!parseBytes(fs, png, "tests/out/splitidat.png") || fs.size() != 1 ||
            !checkPixels(fs, RGBA, 4, 6, 8, "split idat"))
        {
            LOGE("filters: split-IDAT mismatch [%s]", fs.getLastError());
            return false;
        }
    }

    // unsupported row filter: decode failure surfaces as parse error
    {
        const int f[1] = {9};
        CFrameSet fs;
        const std::vector<uint8_t> png = buildFilteredPng(6, f, 1);
        if (parseBytes(fs, png, "tests/out/badfilter.png") ||
            std::string(fs.getLastError()).find("filtering") == std::string::npos)
        {
            LOGE("filters: bad filter accepted [%s]", fs.getLastError());
            return false;
        }
    }

    // corrupt zlib stream with a valid chunk CRC
    {
        const int f[2] = {0, 0};
        CFrameSet fs;
        const std::vector<uint8_t> png = buildFilteredPng(6, f, 2, true);
        if (parseBytes(fs, png, "tests/out/badzlib.png") ||
            std::string(fs.getLastError()).find("Zlib") == std::string::npos)
        {
            LOGE("filters: corrupt zlib accepted [%s]", fs.getLastError());
            return false;
        }
    }
    return true;
}

/**
 * @brief BUG-C3: chunk payloads must be bounded against their destinations
 *        before any copy, even when the attacker computes a valid CRC.
 *
 * @return true
 * @return false
 */
bool test_png_hostile_chunks()
{
    auto must_reject = [](const std::vector<uint8_t> &png, const char *what,
                          const char *err_substr)
    {
        CFrameSet fs;
        const std::string path = "tests/out/hostile.png";
        if (parseBytes(fs, png, path.c_str()) ||
            std::string(fs.getLastError()).find(err_substr) == std::string::npos)
        {
            LOGE("hostile: %s accepted [%s]", what, fs.getLastError());
            return false;
        }
        return true;
    };

    // chunk declared larger than the fixed destination (valid CRC over oversized payload)
    std::vector<uint8_t> png(std::begin(PNG_SIG), std::end(PNG_SIG));
    appendChunk(png, "IHDR", std::vector<uint8_t>(64, 0xaa));
    appendChunk(png, "IEND", {});
    if (!must_reject(png, "oversized IHDR", "IHDR"))
        return false;

    png.assign(std::begin(PNG_SIG), std::end(PNG_SIG));
    appendChunk(png, "PLTE", std::vector<uint8_t>(801, 0xaa)); // > 768
    appendChunk(png, "IEND", {});
    if (!must_reject(png, "oversized PLTE", "PLTE"))
        return false;

    png.assign(std::begin(PNG_SIG), std::end(PNG_SIG));
    appendChunk(png, "PLTE", std::vector<uint8_t>(10, 0xaa)); // not %3
    appendChunk(png, "IEND", {});
    if (!must_reject(png, "PLTE not a multiple of 3", "PLTE"))
        return false;

    png.assign(std::begin(PNG_SIG), std::end(PNG_SIG));
    appendChunk(png, "tRNS", std::vector<uint8_t>(300, 0xaa)); // > 256
    appendChunk(png, "IEND", {});
    if (!must_reject(png, "oversized tRNS", "tRNS"))
        return false;

    // hostile obLT chunks spliced into an otherwise valid stream (before IEND)
    const std::vector<uint8_t> base = build4bppPng(false);
    auto withOblt = [&base](const std::vector<uint8_t> &obltData)
    {
        std::vector<uint8_t> png(base.begin(), base.end() - 12); // drop IEND chunk
        appendChunk(png, "obLT", obltData);
        appendChunk(png, "IEND", {});
        return png;
    };
    auto le32 = [](std::vector<uint8_t> &v, uint32_t n)
    {
        for (int b = 0; b < 4; ++b)
            v.push_back(static_cast<uint8_t>((n >> (8 * b)) & 0xff));
    };

    std::vector<uint8_t> oblt;
    le32(oblt, 0); le32(oblt, 2); le32(oblt, 1000); // v2, Count 1000
    oblt.insert(oblt.end(), 8, 0x11);               // payload claims 8000 bytes
    if (!must_reject(withOblt(oblt), "obLT v2 Count overrun", "obLT"))
        return false;

    oblt.clear();
    le32(oblt, 0); le32(oblt, 0); le32(oblt, 1000); // v0, Count 1000
    oblt.insert(oblt.end(), 8, 0x11);
    if (!must_reject(withOblt(oblt), "obLT v0 Count overrun", "obLT"))
        return false;

    oblt.clear();
    oblt.resize(8, 0); // shorter than the 12-byte obLT header
    if (!must_reject(withOblt(oblt), "truncated obLT header", "obLT"))
        return false;

    // negative and absurd chunk lengths
    png.assign(std::begin(PNG_SIG), std::end(PNG_SIG));
    pushBE(png, 0xffffffffu); // decodes to -1
    png.insert(png.end(), {'I', 'E', 'N', 'D'});
    pushBE(png, 0);
    if (!must_reject(png, "negative chunk size", "invalid chunk size"))
        return false;

    png.assign(std::begin(PNG_SIG), std::end(PNG_SIG));
    pushBE(png, 0x7fffffffu); // larger than the whole file
    png.insert(png.end(), {'I', 'D', 'A', 'T'});
    png.insert(png.end(), 4, 0);
    pushBE(png, 0);
    if (!must_reject(png, "chunk size beyond file", "invalid chunk size"))
        return false;

    // obLT v2 rects must stay inside the frame: explode() fails soft and
    // parsePNG propagates it instead of throwing std::out_of_range
    oblt.clear();
    le32(oblt, 0); le32(oblt, 2); le32(oblt, 1); // v2, Count 1
    const uint16_t badUnit[4] = {4, 0, 8, 8};    // x + sx = 12 > frame width 8
    for (const uint16_t v : badUnit)
    {
        oblt.push_back(static_cast<uint8_t>(v & 0xff));
        oblt.push_back(static_cast<uint8_t>(v >> 8));
    }
    if (!must_reject(withOblt(oblt), "obLT v2 unit outside frame", "obLT v2 unit"))
        return false;

    // hostile IHDR width beyond CFrame::MAX_FRAME_DIM must be rejected
    // before the frame is allocated (patch IHDR width 8 -> 65536 + CRC)
    std::vector<uint8_t> huge = base; // sig(8) size(4) type(4) data(13) crc(4)
    huge[16] = 0x00;
    huge[17] = 0x01; // width = 65536
    huge[18] = 0;
    huge[19] = 0;
    const uint32_t c = static_cast<uint32_t>(crc32(0, huge.data() + 12, 4 + 13));
    huge[29] = static_cast<uint8_t>(c >> 24);
    huge[30] = static_cast<uint8_t>(c >> 16);
    huge[31] = static_cast<uint8_t>(c >> 8);
    huge[32] = static_cast<uint8_t>(c);
    if (!must_reject(huge, "IHDR width beyond MAX_FRAME_DIM", "dimensions too large"))
        return false;

    // the valid fixture (incl. a legal obLT v2) must still parse
    const std::vector<uint8_t> valid = build4bppPng(true);
    CFrameSet fs;
    if (!parseBytes(fs, valid, "tests/out/hostile_ok.png") || fs.size() != 2)
    {
        LOGE("hostile: valid obLT v2 stream rejected [%s]", fs.getLastError());
        return false;
    }
    return true;
}
