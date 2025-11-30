#pragma once
#include <SDL3/SDL.h>
#include <cstdint>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>

class BitmapFont8x8
{
public:
    BitmapFont8x8() = default;
    ~BitmapFont8x8() { destroy(); }

    // Load raw 2048-byte file: 256 glyphs × 8 bytes each (8x8 bits)
    bool loadRawFont(const char *filename)
    {
        auto size = std::filesystem::file_size(filename);
        std::ifstream f(filename, std::ios::binary);
        if (!f)
        {
            std::cerr << "Could not open font: " << filename << "\n";
            return false;
        }
        // f.read(reinterpret_cast<char *>(raw), sizeof(raw));
        f.read(reinterpret_cast<char *>(raw), size);
        return f.good();
    }

    // Build 128x128 atlas texture
    bool buildAtlas(SDL_Renderer *renderer)
    {
        destroy();
        if (!renderer)
            return false;

        const int W = COLS * FONT_SIZE;
        const int H = ROWS * FONT_SIZE;

        // RGBA8888 pixels
        std::vector<uint32_t> pixels(W * H, 0);

        // Fill atlas: 16x16 grid of 8x8 glyphs
        for (int ch = 0; ch < CHARS; ++ch)
        {
            int gx = (ch % COLS) * FONT_SIZE;
            int gy = (ch / COLS) * FONT_SIZE;
            for (int row = 0; row < FONT_SIZE; ++row)
            {
                uint8_t bits = raw[ch][row];
                for (int col = 0; col < FONT_SIZE; ++col)
                {
                    bool on = bits & (1 << col);
                    if (on)
                    {
                        int px = gx + col;
                        int py = gy + row;
                        uint32_t rgba = 0xFFFFFFFF; // white pixel
                        pixels[py * W + px] = rgba;
                    }
                }
            }
        }

        // Create texture
        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, W);
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, H);
        SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_RGBA8888);

        m_texture = SDL_CreateTextureWithProperties(renderer, props);
        SDL_DestroyProperties(props);

        if (!m_texture)
        {
            std::cerr << "Texture creation failed: " << SDL_GetError() << "\n";
            return false;
        }

        SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_NEAREST);

        SDL_SetTextureBlendMode(m_texture, SDL_BLENDMODE_BLEND);
        if (!SDL_UpdateTexture(m_texture, nullptr, pixels.data(), W * 4))
        {
            std::cerr << "SDL_UpdateTexture failed: " << SDL_GetError() << "\n";
            destroy();
            return false;
        }

        return true;
    }

    void destroy()
    {
        if (m_texture)
        {
            SDL_DestroyTexture(m_texture);
            m_texture = nullptr;
        }
    }

    // Draw a single glyph at (x,y)
    void drawChar(SDL_Renderer *r, unsigned char c, float x, float y, float scaleH = 1.0f, float scaleV = 1.0f, const SDL_Color *bgColor = nullptr)
    {
        if (!m_texture)
            return;

        if (bgColor)
        {
            // Save current draw color
            Uint8 oldR, oldG, oldB, oldA;
            SDL_GetRenderDrawColor(r, &oldR, &oldG, &oldB, &oldA);
            SDL_SetRenderDrawColor(r, bgColor->r, bgColor->g, bgColor->b, bgColor->a);
            SDL_FRect bgRect{x, y, static_cast<float>(FONT_SIZE) * scaleH, static_cast<float>(FONT_SIZE) * scaleV};
            // SDL_RenderFillRect accepts SDL_FRect* in SDL3
            SDL_RenderFillRect(r, &bgRect);
            // Restore old draw color
            SDL_SetRenderDrawColor(r, oldR, oldG, oldB, oldA);
        }

        int sx = (c % COLS) * FONT_SIZE;
        int sy = (c / COLS) * FONT_SIZE;

        SDL_FRect src{(float)sx, (float)sy, FONT_SIZE, FONT_SIZE};
        SDL_FRect dst{x, y, static_cast<float>(FONT_SIZE) * scaleH, static_cast<float>(FONT_SIZE) * scaleV};

        SDL_RenderTexture(r, m_texture, &src, &dst);
    }

    // Draw string left-to-right
    void drawText(SDL_Renderer *r, const char *text, const SDL_Color color, const SDL_Color *bgColor, float x, float y, float scaleH = 1.0f, float scaleV = 1.0f)
    {
        SDL_SetTextureColorMod(m_texture, color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(m_texture, color.a);
        float cx = x;
        while (*text)
        {
            unsigned char c = *text++;
            if (c == '\n')
            {
                y += static_cast<float>(FONT_SIZE) * scaleV;
                cx = x;
                continue;
            }
            drawChar(r, c - ' ', cx, y, scaleH, scaleV, bgColor);
            cx += static_cast<float>(FONT_SIZE) * scaleH;
        }
    }

private:
    enum
    {
        COLS = 16,
        ROWS = 16,
        FONT_SIZE = 8,
        CHARS = 95
    };
    uint8_t raw[CHARS][FONT_SIZE]{};
    SDL_Texture *m_texture = nullptr;
};
