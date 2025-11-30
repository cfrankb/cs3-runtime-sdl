#include "tileset_tex.h"
#include "shared/Frame.h"
#include "shared/FrameSet.h"
#include "shared/FileWrap.h"
#include "shared/PngMagic.h"
#include "logger.h"

TileSet::TileSet()
{
    m_texture = nullptr;
}

TileSet ::~TileSet()
{
    if (m_texture)
        SDL_DestroyTexture(m_texture);
}

bool TileSet::load(SDL_Renderer *renderer, const std::string_view &filename, const int sx, const int sy)
{
    LOGI("extracting texture from %s", filename.data());
    CFileWrap file;
    if (!file.open(filename, "rb"))
    {
        LOGE("can't open %s", filename.data());
        return false;
    }

    CFrameSet set;
    if (!parsePNG(set, file, 0, true))
    {
        LOGE("fail to parse %s", filename.data());
        return false;
    }
    file.close();

    return load(renderer, set[0], sx, sy);
}

bool TileSet::load(SDL_Renderer *renderer, CFrame *frame, const int sx, const int sy)
{
    m_tiles.clear();
    m_texture = createTexture(renderer, frame);
    if (!m_texture)
        return false;

    int rows = frame->height() / sy;
    int cols = frame->width() / sx;
    m_tiles.reserve(rows * cols);
    for (float y = 0; y < rows; ++y)
    {
        for (float x = 0; x < cols; ++x)
        {
            SDL_FRect rect{x * sx, y * sy, static_cast<float>(sx), static_cast<float>(sy)};
            m_tiles.emplace_back(std::move(Tile{m_texture, rect}));
        }
    }

    LOGI("TileSet::load() count %lu", m_tiles.size());

    return true;
}

SDL_Texture *TileSet::createTexture(SDL_Renderer *renderer, CFrame *frame)
{
    // --- Create a texture with SDL_PIXELFORMAT_ABGR8888 pixels ---
    SDL_PropertiesID texProps = SDL_CreateProperties();
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, frame->width());
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, frame->height());
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_ABGR8888);
    SDL_Texture *texture = SDL_CreateTextureWithProperties(renderer, texProps);
    SDL_DestroyProperties(texProps);

    if (!texture)
        return nullptr;

    int pitch = frame->width() * sizeof(uint32_t); // 4 bytes per pixel
    if (!SDL_UpdateTexture(texture, nullptr, frame->getRGB().data(), pitch))
    {
        LOGE("SDL_UpdateTexture failed: %s", SDL_GetError());
    }
    return texture;
}
