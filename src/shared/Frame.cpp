/*
    LGCK Builder Runtime
    Copyright (C) 1999, 2011  Francois Blanchette

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

// Frame.cpp : implementation file
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <zlib.h>
#include "Frame.h"
#include "FrameSet.h"
#include "DotArray.h"
#include "CRC.h"
#include "IFile.h"
#include "helper.h"
#include <cstdint>
#include "logger.h"

/////////////////////////////////////////////////////////////////////////////
// CFrame

CFrame::~CFrame()
{
    forget();
}

CFrame::CFrame(int p_nLen, int p_nHei)
{
    m_bCustomMap = false;
    m_nLen = p_nLen;
    m_nHei = p_nHei;

    // generate blank bitmap
    m_rgb = new uint32_t[m_nLen * m_nHei];
    if (!m_rgb)
    {
        LOGE("failed memory allocation\n");
    }
    memset(m_rgb, 0, m_nLen * m_nHei * 4);

    // generate blank map
    m_map.resize(m_nLen, m_nHei);
    m_bCustomMap = false;

    m_undoFrames = nullptr;
    m_undoSize = 0;
    m_undoPtr = 0;
}

CFrame::CFrame(CFrame *src)
{
    if (src)
    {
        m_nHei = src->m_nHei;
        m_nLen = src->m_nLen;
        m_rgb = new uint32_t[m_nLen * m_nHei];
        m_map.resize(src->m_nLen, src->m_nHei);
        m_bCustomMap = src->m_bCustomMap;

        if (src->m_rgb)
        {
            memcpy(m_rgb, src->m_rgb, m_nLen * m_nHei * sizeof(uint32_t));
        }
        else
        {
            // generate blank bitmap
            memset(m_rgb, 0, m_nLen * m_nHei * sizeof(uint32_t));
        }

        m_map = src->getMap();
    }
    else
    {
        init();
    }
    // create undo buffer
    m_undoFrames = nullptr;
    m_undoSize = 0;
    m_undoPtr = 0;
}

CFrame &CFrame::operator=(const CFrame &src)
{
    m_nHei = src.m_nHei;
    m_nLen = src.m_nLen;
    m_rgb = new uint32_t[m_nLen * m_nHei];
    m_map = src.getMap();
    if (src.getRGB())
    {
        memcpy(m_rgb, src.getRGB(), m_nLen * m_nHei * sizeof(uint32_t));
    }
    else
    {
        memset(m_rgb, 0, m_nLen * m_nHei * sizeof(uint32_t));
    }

    m_bCustomMap = src.m_bCustomMap;
    m_map = src.getMap();

    return *this;
}

void CFrame::init()
{
    m_nHei = 0;
    m_nLen = 0;
    m_bCustomMap = false;
    m_rgb = nullptr;
}

void CFrame::forget()
{
    if (m_rgb != nullptr)
    {
        delete[] m_rgb;
        m_rgb = nullptr;
    }

    if (m_undoFrames)
    {
        for (int i = 0; i < m_undoSize; ++i)
        {
            delete m_undoFrames[i];
        }
        delete[] m_undoFrames;
        m_undoFrames = nullptr;
        m_undoSize = 0;
        m_undoPtr = 0;
    }

    m_nLen = 0;
    m_nHei = 0;
}

bool CFrame::write(IFile &file)
{
    file.write(&m_nLen, 4);
    file.write(&m_nHei, 4);
    // OBL5 0x500 expects this field to be 0
    // as it is reserved for future use.
    // int filler = 0;
    file.write(&m_bCustomMap, 4);

    if ((m_nLen > 0) && (m_nHei > 0))
    {
        uLong nDestLen;
        uint8_t *pDest;
        int err = compressData((uint8_t *)m_rgb, 4 * m_nLen * m_nHei, &pDest, nDestLen);
        if (err != Z_OK)
        {
            LOGE("CFrame::write error: %d\n", err);
            return false;
        }

        file.write(&nDestLen, 4);
        file.write(pDest, nDestLen);
        delete[] pDest;

        if (m_bCustomMap)
        {
            m_map.write(file);
        }
    }
    return true;
}

bool CFrame::read(IFile &file, int version)
{
    // clear existing bitmap and map
    if (m_rgb)
    {
        delete[] m_rgb;
        m_rgb = nullptr;
    }

    m_nLen = 0;
    m_nHei = 0;

    if (version == 0x500)
    {
        // read image size and customMap settings
        file.read(&m_nLen, sizeof(m_nLen));
        file.read(&m_nHei, sizeof(m_nHei));
        //    int filler; // OBL5 0x500 expects this field to be 0
        file.read(&m_bCustomMap, sizeof(m_bCustomMap));

        if ((m_nLen > 0) && (m_nHei > 0))
        {
            // this image is not zero-length

            uint32_t nSrcLen;
            file.read(&nSrcLen, 4);

            uint8_t *pSrc = new uint8_t[nSrcLen];
            file.read(pSrc, nSrcLen);

            uLong nDestLen = m_nLen * m_nHei * 4;

            // create a new bitmap
            m_rgb = new uint32_t[m_nLen * m_nHei];

            int err = uncompress(
                (uint8_t *)m_rgb,
                (uLong *)&nDestLen,
                (uint8_t *)pSrc,
                (uLong)nSrcLen);

            delete[] pSrc;

            if (err)
            {
                LOGE("CFrame::read - decompr error: %d", err);
                return false;
            }

            // create a new map
            m_map.resize(m_nLen, m_nHei);
            if (m_bCustomMap)
            {
                m_map.read(file);
            }
            else
            {
                updateMap();
            }
        }
    }
    else
    {
        LOGE("CFrame::read version (=%.4x) not supported\n", version);
        return false;
    }

    return true;
}

void CFrame::toBmp(uint8_t *&bmp, int &totalSize)
{
    // BM was read separatly
    typedef struct
    {
        // uint8_t m_sig;        // "BM"
        uint32_t m_nTotalSize; // 3a 00 00 00
        uint32_t m_nZero;      // 00 00 00 00 ???
        uint32_t m_nDiff;      // 36 00 00 00 TotalSize - ImageSize
        uint32_t m_n28;        // 28 00 00 00 ???

        uint32_t m_nLen;      // 80 00 00 00
        uint32_t m_nHei;      // 80 00 00 00
        int16_t m_nPlanes;    // 01 00
        int16_t m_nBitCount;  // 18 00
        uint32_t m_nCompress; // 00 00 00 00

        uint32_t m_nImageSize; // c0 00 00 00
        uint32_t m_nXPix;      // 00 00 00 00 X pix/m
        uint32_t m_nYPix;      // 00 00 00 00 Y pix/m
        uint32_t m_nClrUsed;   // 00 00 00 00 ClrUsed

        uint32_t m_nClrImpt; // 00 00 00 00 ClrImportant
    } USER_BMPHEADER;

    int pitch = m_nLen * 3;
    if (pitch % 4)
    {
        pitch = pitch - (pitch % 4) + 4;
    }

    totalSize = bmpDataOffset + pitch * m_nHei;

    bmp = new uint8_t[totalSize];
    bmp[0] = 'B';
    bmp[1] = 'M';

    USER_BMPHEADER &bmpHeader = *((USER_BMPHEADER *)(bmp + 2));
    bmpHeader.m_nTotalSize = totalSize;
    bmpHeader.m_nZero = 0;
    bmpHeader.m_nDiff = bmpDataOffset;
    bmpHeader.m_n28 = bmpHeaderSize;

    bmpHeader.m_nLen = m_nLen;
    bmpHeader.m_nHei = m_nHei;
    bmpHeader.m_nPlanes = 1; // always 1
    bmpHeader.m_nBitCount = 24;
    bmpHeader.m_nCompress = 0; // 00 00 00 00

    bmpHeader.m_nImageSize = pitch * m_nHei;
    bmpHeader.m_nXPix = 0;    // 00 00 00 00 X pix/m
    bmpHeader.m_nYPix = 0;    // 00 00 00 00 Y pix/m
    bmpHeader.m_nClrUsed = 0; // 00 00 00 00 ClrUsed
    bmpHeader.m_nClrImpt = 0; // 00 00 00 00 ClrImportant

    for (int y = 0; y < m_nHei; ++y)
    {
        for (int x = 0; x < m_nLen; ++x)
        {
            uint8_t *s = (uint8_t *)&point(x, m_nHei - y - 1);
            uint8_t *d = bmp + bmpDataOffset + x * 3 + y * pitch;
            d[0] = s[2];
            d[1] = s[1];
            d[2] = s[0];
        }
    }
}

uint32_t CFrame::toNet(const uint32_t a)
{
    uint32_t b;
    uint8_t *s = (uint8_t *)&a;
    uint8_t *d = (uint8_t *)&b;

    d[0] = s[3];
    d[1] = s[2];
    d[2] = s[1];
    d[3] = s[0];

    return b;
}

bool CFrame::toPng(uint8_t *&png, int &totalSize, uint8_t *obl5data, int obl5size)
{
    CCRC crc;

    // compress the data ....................................
    int scanLine = m_nLen * 4;
    uLong dataSize = (scanLine + 1) * m_nHei;
    uint8_t *data = new uint8_t[dataSize];
    for (int y = 0; y < m_nHei; ++y)
    {
        uint8_t *d = data + y * (scanLine + 1);
        *d = 0;
        memcpy(d + 1, m_rgb + y * m_nLen, scanLine);
    }

    uint8_t *cData;
    uLong cDataSize;
    int err = compressData(data, dataSize, &cData, cDataSize);
    if (err != Z_OK)
    {
        LOGE("CFrame::toPng error: %d\n", err);
        return true;
    }

    delete[] data;

    int cDataBlocks = cDataSize / pngChunkLimit;
    if (cDataSize % pngChunkLimit)
    {
        cDataBlocks++;
    }

    totalSize = pngHeaderSize + png_IHDR_Size + 4 + cDataSize + 12 * cDataBlocks + sizeof(png_IEND) + obl5size;

    png = new uint8_t[totalSize];
    uint8_t *t = png;

    // png signature ---------------------------------------
    uint8_t sig[] = {137, 80, 78, 71, 13, 10, 26, 10};
    memcpy(t, sig, 8);
    t += 8;

    uint32_t crc32;

    // png_IHDR ---------------------------------------------
    png_IHDR &ihdr = *((png_IHDR *)t);
    ihdr.Lenght = toNet(png_IHDR_Size - 8);
    memcpy(ihdr.ChunkType, "IHDR", 4);
    ihdr.Width = toNet(m_nLen);
    ihdr.Height = toNet(m_nHei);
    ihdr.BitDepth = 8;
    ihdr.ColorType = 6;
    ihdr.Compression = 0; // deflated
    ihdr.Filter = 0;
    ihdr.Interlace = 0;
    // ihdr.CRC = 0;
    t += png_IHDR_Size;
    crc32 = toNet(crc.crc(((uint8_t *)&ihdr) + 4, png_IHDR_Size - 4));
    memcpy(t, &crc32, 4);
    t += 4;

    // png_IDAT ....................................................
    uint32_t cDataOffset = 0;
    uint32_t cDataLeft = cDataSize;
    do
    {
        int chunkSize;
        if (cDataLeft > pngChunkLimit)
        {
            chunkSize = pngChunkLimit;
        }
        else
        {
            chunkSize = cDataLeft;
        }

        uint32_t cDataSizeNet = toNet(chunkSize);
        memcpy(t, &cDataSizeNet, 4);
        t += 4;
        uint8_t *chunkData = t;
        memcpy(t, "IDAT", 4);
        t += 4;
        memcpy(t, cData + cDataOffset, chunkSize);
        t += chunkSize;
        crc32 = toNet(crc.crc(chunkData, chunkSize + 4));
        memcpy(t, &crc32, 4);
        t += 4;

        cDataOffset += chunkSize;
        cDataLeft -= chunkSize;
    } while (cDataLeft);

    // png_obLT
    if (obl5size)
    {
        memcpy(t, obl5data, obl5size);
        png_OBL5 &obl5 = *((png_OBL5 *)t);
        obl5.Length = toNet(obl5size - 12);
        uint32_t iCrc = toNet(crc.crc(t + 4, obl5size - 8));
        memcpy(t + obl5size - 4, &iCrc, 4);
        t += obl5size;
    }

    // png_IEND .................................................
    png_IEND &iend = *((png_IEND *)t);
    iend.Lenght = 0;
    memcpy(iend.ChunkType, "IEND", 4);
    iend.CRC = toNet(crc.crc((uint8_t *)"IEND", 4));

    delete[] cData;
    return true;
}

void CFrame::resize(int len, int hei)
{
    CFrame *newFrame = new CFrame(len, hei);

    // Copy the original frame
    for (int y = 0; y < std::min(hei, m_nHei); ++y)
    {
        for (int x = 0; x < std::min(len, m_nLen); ++x)
        {
            newFrame->at(x, y) = at(x, y);
        }
    }

    delete[] m_rgb;
    m_rgb = newFrame->getRGB();
    newFrame->detach();
    delete newFrame;

    m_nLen = len;
    m_nHei = hei;

    // TODO: actually resize the map
    m_map.resize(m_nLen, m_nHei);
}

void CFrame::setTransparency(uint32_t color)
{
    color &= COLOR_MASK;
    for (int i = 0; i < m_nLen * m_nHei; ++i)
    {
        if ((m_rgb[i] & COLOR_MASK) == color)
        {
            m_rgb[i] = 0;
        }
    }
}

void CFrame::setTopPixelAsTranparency()
{
    setTransparency(m_rgb[0]);
}

void CFrame::updateMap()
{
    m_map.resize(m_nLen, m_nHei);
    int threhold = CSS3Map::GRID * CSS3Map::GRID / 4;
    for (int y = 0; y < m_map.height(); ++y)
    {
        for (int x = 0; x < m_map.length(); ++x)
        {
            int data = 0;
            for (int i = 0; i < CSS3Map::GRID; ++i)
            {
                for (int j = 0; j < CSS3Map::GRID; ++j)
                {
                    data += ((point(x * CSS3Map::GRID + i, y * CSS3Map::GRID + j)) & ALPHA_MASK) != 0;
                }
            }
            if (data >= threhold)
            {
                m_map.at(x, y) = 0xff;
            }
            else
            {
                m_map.at(x, y) = 0;
            }
        }
    }
}

bool CFrame::hasTransparency() const
{
    for (int i = 0; i < m_nLen * m_nHei; ++i)
    {
        if (!(m_rgb[i] & ALPHA_MASK))
        {
            return true;
        }
    }

    return false;
}

CFrameSet *CFrame::split(int pxSize, bool whole)
{
    CFrameSet *frameSet = new CFrameSet();
    for (int y = 0; y < m_nHei; y += pxSize)
    {
        for (int x = 0; x < m_nLen; x += pxSize)
        {
            CFrame *frame;
            if (whole)
            {
                frame = new CFrame(pxSize, pxSize);
            }
            else
            {
                frame = new CFrame(std::min(pxSize, m_nLen - x), std::min(pxSize, m_nHei - y));
            }
            uint32_t *ni_rgb = frame->getRGB();
            for (int z = 0; z < std::min(pxSize, m_nHei - y); ++z)
            {
                memcpy(&ni_rgb[z * frame->m_nLen], // pxSize],
                       &m_rgb[x + (y + z) * m_nLen],
                       std::min(pxSize, m_nLen - x) * sizeof(uint32_t));
            }
            frameSet->add(frame);
            // TODO: copy the actual map
            frame->updateMap();
        }
    }
    return frameSet;
}

bool CFrame::draw(CDotArray *dots, int size, int mode)
{
    bool changed = false;
    for (size_t i = 0; i < (*dots).getSize(); ++i)
    {
        const Dot &dot = (*dots)[i];
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                if (dot.x + x < m_nLen && dot.y + y < m_nHei)
                {
                    switch (mode)
                    {
                    case MODE_NORMAL:
                        if (at(dot.x + x, dot.y + y) != dot.color)
                        {
                            at(dot.x + x, dot.y + y) = dot.color;
                            changed = true;
                        }
                        break;

                    case MODE_COLOR_ONLY:
                        if ((at(dot.x + x, dot.y + y) & 0xffffff) != (dot.color & 0xffffff))
                        {
                            uint8_t *p = (uint8_t *)&at(dot.x + x, dot.y + y);
                            uint8_t a = p[3];
                            at(dot.x + x, dot.y + y) = dot.color;
                            p[3] = a;
                            changed = true;
                        }
                        break;

                    case MODE_ALPHA_ONLY:
                        if (alphaAt(dot.x + x, dot.y + y) != (dot.color >> 24))
                        {
                            uint8_t *p = (uint8_t *)&at(dot.x + x, dot.y + y);
                            p[3] = dot.color >> 24;
                            changed = true;
                        }
                        break;
                    }
                }
            }
        }
    }
    return changed;
}

void CFrame::save(CDotArray *dots, CDotArray *dotsOrg, int size)
{
    for (size_t i = 0; i < (*dots).getSize(); ++i)
    {
        const Dot &dot = (*dots)[i];
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                if (dot.x + x < m_nLen && dot.y + y < m_nHei)
                {
                    dotsOrg->add(at(dot.x + x, dot.y + y), dot.x + x, dot.y + y);
                }
            }
        }
    }
}

void CFrame::floodFill(int x, int y, uint32_t bOldColor, uint32_t bNewColor)
{
    if (!isValid(x, y))
    {
        return;
    }

    int ex = x;
    for (; (x >= 0) && at(x, y) == bOldColor; --x)
    {
        at(x, y) = bNewColor;
        if ((y > 0) && (at(x, y - 1) == bOldColor))
        {
            floodFill(x, y - 1, bOldColor, bNewColor);
        }

        if ((y < m_nHei - 1) && (at(x, y + 1) == bOldColor))
        {
            floodFill(x, y + 1, bOldColor, bNewColor);
        }
    }

    x = ++ex;
    if (!isValid(x, y))
    {
        return;
    }

    for (; (x < m_nLen) && at(x, y) == bOldColor; ++x)
    {
        at(x, y) = bNewColor;
        if ((y > 0) && (at(x, y - 1) == bOldColor))
        {
            floodFill(x, y - 1, bOldColor, bNewColor);
        }

        if ((y < m_nHei - 1) && (at(x, y + 1) == bOldColor))
        {
            floodFill(x, y + 1, bOldColor, bNewColor);
        }
    }
}

void CFrame::floodFillAlpha(int x, int y, uint8_t oldAlpha, uint8_t newAlpha)
{
    if (!isValid(x, y))
    {
        return;
    }

    int ex = x;
    for (; (x >= 0) && alphaAt(x, y) == oldAlpha; --x)
    {
        uint8_t *p = (uint8_t *)&point(x, y);
        p[3] = newAlpha;
        if ((y > 0) && (alphaAt(x, y - 1) == oldAlpha))
        {
            floodFillAlpha(x, y - 1, oldAlpha, newAlpha);
        }

        if ((y < m_nHei - 1) && (alphaAt(x, y + 1) == oldAlpha))
        {
            floodFillAlpha(x, y + 1, oldAlpha, newAlpha);
        }
    }

    x = ++ex;
    if (!isValid(x, y))
    {
        return;
    }

    for (; (x < m_nLen) && alphaAt(x, y) == oldAlpha; ++x)
    {
        uint8_t *p = (uint8_t *)&point(x, y);
        p[3] = newAlpha;
        if ((y > 0) && (alphaAt(x, y - 1) == oldAlpha))
        {
            floodFillAlpha(x, y - 1, oldAlpha, newAlpha);
        }

        if ((y < m_nHei - 1) && (alphaAt(x, y + 1) == oldAlpha))
        {
            floodFillAlpha(x, y + 1, oldAlpha, newAlpha);
        }
    }
}

CFrame *CFrame::clip(int mx, int my, int cx, int cy)
{
    int maxLen = m_nLen - mx;
    int maxHei = m_nHei - my;

    if (cx == -1)
    {
        cx = maxLen;
    }

    if (cy == -1)
    {
        cy = maxHei;
    }

    // check if out of bound
    if (cx < 1 || cy < 1)
    {
        cx = cy = 0;
    }

    // create clipped frame
    CFrame *t = new CFrame(cx, cy);

    // copy clipped region
    for (int y = 0; y < cy; ++y)
    {
        for (int x = 0; x < cx; ++x)
        {
            t->at(x, y) = at(mx + x, my + y);
        }
    }

    // return new frame
    return t;
}

void CFrame::clear()
{
    for (int y = 0; y < m_nHei; ++y)
    {
        for (int x = 0; x < m_nLen; ++x)
        {
            at(x, y) = 0;
        }
    }

    // TODO: updatemap
}

void CFrame::flipV()
{
    for (int y = 0; y < m_nHei / 2; ++y)
    {
        for (int x = 0; x < m_nLen; ++x)
        {
            uint32_t c = at(x, y);
            at(x, y) = at(x, m_nHei - y - 1);
            at(x, m_nHei - y - 1) = c;
        }
    }
    // TODO: updatemap
}

void CFrame::flipH()
{
    for (int y = 0; y < m_nHei; ++y)
    {
        for (int x = 0; x < m_nLen / 2; ++x)
        {
            uint32_t c = at(x, y);
            at(x, y) = at(m_nLen - x - 1, y);
            at(m_nLen - x - 1, y) = c;
        }
    }
    // TODO: updatemap
}

void CFrame::rotate()
{
    CFrame *newFrame = new CFrame(m_nHei, m_nLen);
    for (int y = 0; y < m_nHei; ++y)
    {
        for (int x = 0; x < m_nLen; ++x)
        {
            newFrame->at(newFrame->m_nLen - y - 1, x) = at(x, y);
        }
    }

    m_nLen = newFrame->m_nLen;
    m_nHei = newFrame->m_nHei;
    delete[] m_rgb;
    m_rgb = newFrame->getRGB();

    newFrame->detach();
    delete newFrame;

    // TODO: updatemap
}

void CFrame::spreadH()
{
    uint32_t *rgb = new uint32_t[m_nLen * m_nHei * 2];

    for (int y = 0; y < m_nHei; ++y)
    {
        memcpy(rgb + y * m_nLen * 2,
               &at(0, y),
               m_nLen * sizeof(uint32_t));

        memcpy(rgb + y * m_nLen * 2 + m_nLen,
               &at(0, y),
               m_nLen * sizeof(uint32_t));
    }

    m_nLen *= 2;
    delete[] m_rgb;
    m_rgb = rgb;

    // TODO: updatemap
}

void CFrame::spreadV()
{
    uint32_t *rgb = new uint32_t[m_nLen * m_nHei * 2];

    for (int y = 0; y < m_nHei; ++y)
    {
        memcpy(rgb + y * m_nLen,
               &at(0, y),
               m_nLen * sizeof(uint32_t));

        memcpy(rgb + y * m_nLen + m_nHei * m_nLen,
               &at(0, y),
               m_nLen * sizeof(uint32_t));
    }

    m_nHei *= 2;
    delete[] m_rgb;
    m_rgb = rgb;

    // TODO: updatemap
}

void CFrame::shrink()
{
    CFrame *newFrame = new CFrame(m_nLen / 2, m_nHei / 2);
    for (int y = 0; y < m_nHei / 2; ++y)
    {
        for (int x = 0; x < m_nLen / 2; ++x)
        {
            newFrame->at(x, y) = at(x * 2, y * 2);
        }
    }

    // why wasn't the old rgb deleted?
    if (m_rgb)
    {
        delete m_rgb;
    }
    m_rgb = newFrame->getRGB();
    newFrame->detach();
    delete newFrame;

    m_nLen /= 2;
    m_nHei /= 2;

    // TODO: actually resize the map
    m_map.resize(m_nLen, m_nHei);

    // TODO: fix this temp updateMap
    updateMap();
}

const CSS3Map &CFrame::getMap() const
{
    return m_map;
}

void CFrame::enlarge()
{
    CFrame *newFrame = new CFrame(m_nLen * 2, m_nHei * 2);
    for (int y = 0; y < m_nHei; ++y)
    {
        for (int x = 0; x < m_nLen; ++x)
        {
            uint32_t c = at(x, y);
            newFrame->at(x * 2, y * 2) = c;
            newFrame->at(x * 2 + 1, y * 2) = c;
            newFrame->at(x * 2, y * 2 + 1) = c;
            newFrame->at(x * 2 + 1, y * 2 + 1) = c;
        }
    }

    m_rgb = newFrame->getRGB();
    newFrame->detach();
    delete newFrame;

    m_nLen *= 2;
    m_nHei *= 2;

    // TODO: actually resize the map
    m_map.resize(m_nLen, m_nHei);

    // TODO: fix this temp updateMap
    updateMap();
}

void CFrame::shiftUP(const bool wrap)
{
    uint32_t *t = new uint32_t[m_nLen];

    // copy first line to buffer
    memcpy(t, m_rgb, sizeof(uint32_t) * m_nLen);

    // shift
    for (int y = 0; y < m_nHei - 1; ++y)
    {
        memcpy(&at(0, y), &at(0, y + 1), m_nLen * sizeof(uint32_t));
    }

    // copy first line to last
    if (wrap)
        memcpy(&at(0, m_nHei - 1), t, sizeof(uint32_t) * m_nLen);
    else
        memset(&at(0, m_nHei - 1), 0, sizeof(uint32_t) * m_nLen);

    delete[] t;
}

void CFrame::shiftDOWN(const bool wrap)
{
    uint32_t *t = new uint32_t[m_nLen];

    // copy first line to buffer
    memcpy(t, &at(0, m_nHei - 1), sizeof(uint32_t) * m_nLen);

    // shift
    for (int y = 0; y < m_nHei - 1; ++y)
    {
        memcpy(&at(0, m_nHei - y - 1), &at(0, m_nHei - y - 2), m_nLen * sizeof(uint32_t));
    }

    // copy first line to last
    if (wrap)
        memcpy(m_rgb, t, sizeof(uint32_t) * m_nLen);
    else
        memset(m_rgb, 0, sizeof(uint32_t) * m_nLen);
    delete[] t;
}

void CFrame::shiftLEFT(const bool wrap)
{
    for (int y = 0; y < m_nHei; ++y)
    {
        const uint32_t c = at(0, y);
        for (int x = 0; x < m_nLen - 1; ++x)
        {
            at(x, y) = at(x + 1, y);
        }
        if (wrap)
            at(m_nLen - 1, y) = c;
        else
            at(m_nLen - 1, y) = 0;
    }
}

void CFrame::shiftRIGHT(const bool wrap)
{
    for (int y = 0; y < m_nHei; ++y)
    {
        const uint32_t c = at(m_nLen - 1, y);
        for (int x = 0; x < m_nLen - 1; ++x)
        {
            at(m_nLen - 1 - x, y) = at(m_nLen - 2 - x, y);
        }
        if (wrap)
            at(0, y) = c;
        else
            at(0, y) = 0;
    }
}

bool CFrame::isEmpty() const
{
    for (int y = 0; y < m_nHei; ++y)
    {
        for (int x = 0; x < m_nLen; ++x)
        {
            if ((m_rgb[x + y * m_nLen] & 0xff000000))
            {
                return false;
            }
        }
    }
    return true;
}

void CFrame::inverse()
{
    for (int y = 0; y < m_nHei; ++y)
    {
        for (int x = 0; x < m_nLen; ++x)
        {
            unsigned int &rgb = at(x, y);
            rgb = (~rgb & 0xffffff) + (rgb & 0xff000000);
        }
    }
}

void CFrame::copy(CFrame *frame)
{
    *this = *frame;
}

void CFrame::undo()
{
    if (m_undoPtr < m_undoSize)
    {
        CFrame *tmp = m_undoFrames[m_undoPtr];
        m_undoFrames[m_undoPtr] = new CFrame(this);
        copy(tmp);
        delete tmp;
        m_undoPtr++;
    }
}

void CFrame::redo()
{
    if (m_undoPtr && m_undoSize)
    {
        CFrame *tmp = m_undoFrames[m_undoPtr - 1];
        m_undoFrames[m_undoPtr - 1] = new CFrame(this);
        copy(tmp);
        delete tmp;
        m_undoPtr--;
    }
}

void CFrame::push()
{
    if (!m_undoFrames)
    {
        m_undoFrames = new CFrame *[MAX_UNDO];
        m_undoPtr = 0;
        m_undoSize = 0;
    }

    if (m_undoPtr != 0)
    {
        for (int i = 0; i < m_undoPtr; ++i)
        {
            delete m_undoFrames[i];
        }

        for (int i = 0; i + m_undoPtr < m_undoSize; ++i)
        {
            m_undoFrames[i] = m_undoFrames[m_undoPtr + i];
        }
        m_undoSize -= m_undoPtr;
        m_undoPtr = 0;
    }

    if (m_undoSize == MAX_UNDO)
    {
        delete m_undoFrames[m_undoSize - 1];
    }

    // push everything back
    for (int i = std::min(m_undoSize, (int)MAX_UNDO - 1); i > 0; --i)
    {
        m_undoFrames[i] = m_undoFrames[i - 1];
    }

    if (m_undoSize < MAX_UNDO)
    {
        ++m_undoSize;
    }

    m_undoFrames[0] = new CFrame(this);
}

bool CFrame::canUndo()
{
    return m_undoFrames != nullptr && m_undoPtr < m_undoSize;
}

bool CFrame::canRedo()
{
    return m_undoFrames != nullptr && m_undoPtr > 0 && m_undoSize;
}

void CFrame::shadow(int factor)
{
    for (int y = 0; y < m_nHei; ++y)
    {
        for (int x = 0; x < m_nLen; ++x)
        {
            unsigned int &rgb = at(x, y);
            rgb = (rgb & 0xffffff) + (rgb & 0xff000000) / factor;
        }
    }
}

void CFrame::fade(int factor)
{
    for (int y = 0; y < m_nHei; ++y)
    {
        for (int x = 0; x < m_nLen; ++x)
        {
            unsigned int &rgb = at(x, y);
            rgb = (rgb & 0xffffff) + ((((rgb & 0xff000000) >> 24) * factor / 255) << 24);
        }
    }
}

CFrameSet *CFrame::explode(int count, short *xx, short *yy, CFrameSet *set)
{
    if (!set)
    {
        set = new CFrameSet();
    }

    int mx = 0;
    for (int i = 0; i < count; ++i)
    {
        CFrame *frame = clip(mx, 0, xx[i], yy[i]);
        frame->updateMap();
        set->add(frame);
        mx += xx[i];
    }
    return set;
}

void CFrame::abgr2argb()
{
    // swap blue/red
    for (int i = 0; i < m_nLen * m_nHei; ++i)
    {
        uint32_t t = (m_rgb[i] & 0xff00ff00);
        if (t & 0xff000000)
        {
            t += ((m_rgb[i] & 0xff) << 16) + ((m_rgb[i] & 0xff0000) >> 16);
        }
        m_rgb[i] = t;
    }
}

void CFrame::argb2arbg()
{
    // swap green/blue
    for (int i = 0; i < m_nLen * m_nHei; ++i)
    {
        uint32_t t = (m_rgb[i] & 0xff0000ff);
        if (t & 0xff000000)
        {
            t += ((m_rgb[i] & 0xff00) << 8) + ((m_rgb[i] & 0xff0000) >> 8);
        }
        m_rgb[i] = t;
    }
}

CFrame *CFrame::toAlphaGray(int mx, int my, int cx, int cy)
{
    CFrame *frame;

    if (mx != 0 || my != 0 || cx != -1 || cy != -1)
    {
        frame = this->clip(mx, my, cx, cy);
    }
    else
    {
        frame = new CFrame(this);
    }

    for (int i = 0; i < frame->m_nLen * frame->m_nHei; ++i)
    {
        unsigned char *p = (unsigned char *)(frame->m_rgb + i);
        p[0] = p[3];
        p[1] = p[3];
        p[2] = p[3];
        p[3] = 0xff;
    }
    return frame;
}

const char *CFrame::getChunkType()
{
    return "obLT";
}

void CFrame::fill(unsigned int rgba)
{
    for (int i = 0; i < m_nLen * m_nHei; ++i)
    {
        m_rgb[i] = rgba;
    }
}

void CFrame::drawAt(CFrame &frame, int bx, int by, bool tr)
{
    for (int y = 0; y < frame.m_nHei; ++y)
    {
        if (by + y >= m_nHei)
        {
            break;
        }
        for (int x = 0; x < frame.m_nLen; ++x)
        {
            if (bx + x >= m_nLen)
            {
                break;
            }
            if (!tr || frame.at(x, y))
                at(bx + x, by + y) = frame.at(x, y);
        }
    }
}

uint32_t *CFrame::swapBuffer(uint32_t *newBuffer)
{
    uint32_t *tmp = m_rgb;
    m_rgb = newBuffer;
    return tmp;
}

/////////////////////////////////////////////////////////////////////
// CSS3Map

CSS3Map::CSS3Map()
{
    m_map = nullptr;
    m_len = 0;
    m_hei = 0;
}

CSS3Map::CSS3Map(const int px, const int py)
{
    m_map = nullptr;
    resize(px, py);
}

CSS3Map::~CSS3Map()
{
    if (m_map)
    {
        delete[] m_map;
    }
}

void CSS3Map::resize(const int px, const int py)
{
    if (m_map)
    {
        delete[] m_map;
    }

    m_len = px / GRID;
    m_hei = py / GRID;
    m_map = new char[m_len * m_hei];
    memset(m_map, 0, m_len * m_hei);
}

int CSS3Map::length() const
{
    return m_len;
}

int CSS3Map::height() const
{
    return m_hei;
}

void CSS3Map::read(IFile &file)
{
    // TODO: read map from disk
    file.read(m_map, m_len * m_hei);
}

void CSS3Map::write(IFile &file) const
{
    // TODO: write map to disk
    file.write(m_map, m_len * m_hei);
}

bool CSS3Map::isNULL() const
{
    return !m_map;
}

CSS3Map &CSS3Map::operator=(const CSS3Map &src)
{
    resize(src.length(), src.height());
    if (!src.isNULL())
    {
        memcpy(m_map, src.getMap(), m_len * m_hei);
    }
    return *this;
}

char *CSS3Map::getMap() const
{
    return m_map;
}
