#include <string>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <format>
#include <cmath>
#include <set>
#include "../../../src/shared/FileWrap.h"
#include "../../../src/logger.h"
#include "../../../src/strhelper.h"
#include "../../../src/shared/FrameSet.h"
#include "../../../src/shared/Frame.h"
#include "../../../src/logger.h"

namespace SpriteSheet
{
    constexpr int NOT_FOUND = -1;
    constexpr uint32_t OBLT_VERSION = 2;
    constexpr uint32_t PNG_STANDARD_HDR_SIZE = 12;
    constexpr int SIZE_ALIGNMENT = 8;
    constexpr int SIZE_ADJUSTMENT = 16;

    inline bool between(int a1, int a2, int b1, int b2)
    {
        return a1 < b2 && a2 > b1;
    }

    enum Flag
    {
        noflag = 0,
        sorted = 1,
    };
}

struct Rect
{
    int x;
    int y;
    int width;
    int height;
    uint32_t color;
};

struct Sprite
{
    int x;
    int y;
    int width;
    int height;
    CFrame *frame;
    int id;
};

struct Size
{
    int sx;
    int sy;
};

void appendBytes(std::vector<uint8_t> &vec, const uint8_t *data, size_t size)
{
    vec.insert(vec.end(), data, data + size);
}

bool extractFrameSet(CFrameSet &set, const std::string_view &filepath)
{
    CFileWrap file;
    if (file.open(filepath.data()))
    {
        if (!set.extract(file))
        {
            LOGE("failed to read: %s [%s]", filepath.data(), set.getLastError());
            return false;
        }
        // LOGI("%s >>> size: %lu", item.c_str(), set.getSize());
    }
    else
    {
        LOGE("failed open %s", filepath.data());
        return false;
    }

    return true;
}

bool getTestData(CFrameSet &set, const std::string_view &filepath)
{
    std::vector<std::string> list;
    CFileWrap file;
    if (file.open(filepath))
    {
        size_t size = file.getSize();
        std::vector<uint8_t> buf(size);
        file.read(buf.data(), size);
        std::string data;
        data.assign(reinterpret_cast<char *>(buf.data()), buf.size());
        splitString2(data, list);
        file.close();
    }
    else
    {
        LOGE("can't open: %s", filepath.data());
        return false;
    }

    size_t last = 0;
    for (const auto &item : list)
    {
        if (item == "list.txt")
            continue;
        // LOGI("item:%s", item.c_str());
        std::string filepath = "data/" + item;
        if (extractFrameSet(set, filepath))
        {
            if (set.getSize() - last == 0)
            {
                LOGW("empty set???");
            }
            if (set.getSize() < last)
            {
                LOGW("bad set???");
            }
            last = set.getSize();
        }
    }
    return true;
}

void drawRect(CFrame &frame, const Rect &rect, const uint32_t color, bool fill)
{
    uint32_t *rgba = frame.getRGB().data();
    const int rowPixels = frame.width();
    if (fill)
    {
        for (int y = 0; y < rect.height; y++)
        {
            for (int x = 0; x < rect.width; x++)
            {
                rgba[(rect.y + y) * rowPixels + rect.x + x] = color;
            }
        }
    }
    else
    {
        for (int y = 0; y < rect.height; y++)
        {
            for (int x = 0; x < rect.width; x++)
            {
                if (y == 0 || y == rect.height - 1 || x == 0 || x == rect.width - 1)
                {
                    rgba[(rect.y + y) * rowPixels + rect.x + x] = color;
                }
            }
        }
    }
}

/**
 * @brief
 *
 * @param rects
 * @param frame
 * @return int
 */
int findBestMatchRect(std::vector<Rect> &rects, CFrame *frame)
{
    int sm = SpriteSheet::NOT_FOUND;
    int i = 0;
    for (const auto &rect : rects)
    {
        if (rect.width >= frame->width() && rect.height >= frame->height())
        {
            if (sm == SpriteSheet::NOT_FOUND)
            {
                sm = i;
            }
            else
            {
                int sxRect = rect.width * rect.height;
                int sxRectSM = rects[sm].width * rects[sm].height;
                if (rect.height < rects[sm].height)
                {
                    sm = i;
                }
                else if (rect.height == rects[sm].height && (rect.y < rects[sm].y))
                {
                    sm = i;
                }
                else if (sxRect < sxRectSM && (rect.y < rects[sm].y))
                {
                    sm = i;
                }
            }
        }
        ++i;
    }
    return sm;
}

void printRect(const Rect &rect, std::string_view name)
{
    LOGI("[%s].x=%d .y=%d .w=%d h=%d", name.data(), rect.x, rect.y, rect.width, rect.height);
}

/**
 * @brief check if two sprites have been accidently placed over each other
 *         this is a sanity check. sanity checks will be moved
 *          to unit tests later
 *
 * @param sprites
 * @return true
 * @return false
 */
bool checkOverlaps(std::vector<Sprite> &sprites)
{
    for (size_t i = 0; i < sprites.size(); ++i)
    {
        for (size_t j = 0; j < sprites.size(); ++j)
        {
            if (i == j)
                continue;

            Sprite &a = sprites[i];
            Sprite &b = sprites[j];
            if (SpriteSheet::between(a.x, a.x + a.width, b.x, b.x + b.width) &&
                SpriteSheet::between(a.y, a.y + a.height, b.y, b.y + b.height))
            {
                LOGE("intersection between pixmap %d and %d", i, j);
                return false;
            }
        }
    }
    return true;
}

/**
 * @brief Verify that there is no overlap by comparing the pixel content of the sheet
 *      with individual frame. this is another sanity check. sanity checks will be moved
 *      to unit tests later
 *
 * @param bitmap
 * @param sprites
 * @return true
 * @return false
 */
bool checkPixmap(CFrame &bitmap, std::vector<Sprite> &sprites)
{
    int i = 0;
    for (const auto &sprite : sprites)
    {
        for (int y = 0; y < sprite.height; ++y)
        {
            for (int x = 0; x < sprite.width; ++x)
            {
                if (sprite.x == SpriteSheet::NOT_FOUND || sprite.y == SpriteSheet::NOT_FOUND)
                {
                    LOGE("unplaced sprite at index %d [%d x %d]", i, sprite.frame->width(), sprite.frame->height());
                    return false;
                }

                if (sprite.x + x > bitmap.width() || sprite.y + y > bitmap.width())
                {
                    LOGE("trying to write sprite %d at (%lu,%lu) -- outside sprite sheet [%d, %d]",
                         sprite.x + x, sprite.y + y, bitmap.width(), bitmap.width());
                    return false;
                }

                if (bitmap.at(sprite.x + x, sprite.y + y) != sprite.frame->at(x, y))
                {
                    LOGE("pixmap for sprite %d at (%lu,%lu) mistmatch [0x%.8x] -- expected [0x%.8x]",
                         i, x, y, bitmap.at(sprite.x + x, sprite.y + y), sprite.frame->at(x, y));
                    return false;
                }
            }
        }
        ++i;
    }
    return true;
}

void createSheet(CFrame &bitmap, const std::vector<Sprite> &sprites, const std::vector<Rect> &rects)
{
    int i = 0;
    for (const auto &sprite : sprites)
    {
        if (sprite.x != SpriteSheet::NOT_FOUND && sprite.y != SpriteSheet::NOT_FOUND)
        {
            if (sprite.x + sprite.width > bitmap.width())
            {
                LOGW("sprite %d outside bitmap bound; x=%d y=%d [%d, %d]; expected < [%d, %d]",
                     i, sprite.x, sprite.y, sprite.x + sprite.width, sprite.y + sprite.height, bitmap.width(), bitmap.height());
                continue;
            }
            if (sprite.y + sprite.height > bitmap.height())
            {
                LOGW("sprite %d outside bitmap bound; x=%d y=%d [%d, %d]; expected < [%d, %d]",
                     i, sprite.x, sprite.y, sprite.x + sprite.width, sprite.y + sprite.height, bitmap.width(), bitmap.height());
                continue;
            }
            if (sprite.frame == nullptr)
            {
                LOGW("sprite.frame == nullptr for sprite %d", i);
                continue;
            }
            bitmap.drawAt(*sprite.frame, sprite.x, sprite.y, false);
        }
        else
        {
            LOGW("unplaced sprite at index %d [%d x %d]", i, sprite.frame->width(), sprite.frame->height());
        }
        ++i;
    }

    i = 0;
    for (const auto &rect : rects)
    {
        if (rect.x + rect.width < bitmap.width() && rect.y + rect.height < bitmap.height())
        {
            drawRect(bitmap, rect, rect.color, true);
        }
        else
        {
            LOGW("rect%d x=%d y=%d span %d %d -- outside bound %d, %d", i,
                 rect.x, rect.y, rect.x + rect.width, rect.y + rect.height,
                 bitmap.width(), bitmap.height());
            if (rect.x > bitmap.width() || rect.y > bitmap.height())
                continue;
            Rect t{.x = rect.x, .y = rect.y, .width = std::min(rect.width, bitmap.width() - rect.x), .height = std::min(rect.height, bitmap.height() - rect.y), .color = rect.color};
            drawRect(bitmap, t, t.color, true);
        }
        ++i;
    }
}

uint32_t getRandomColor()
{
    return rand() | 0xff000000;
}

void printRects(const std::vector<Rect> &rects)
{
    int j = 0;
    for (const auto &rect : rects)
    {
        printRect(rect, "rect" + std::to_string(j));
        ++j;
    }
}

int64_t estimateSheetSize(CFrameSet &set)
{
    // constexpr const int SIZE_ALIGNMENT = 8;   // Align size to 8 pixels
    // constexpr const int SIZE_ADJUSTMENT = 16; // Adjustment factor for sheet size
    uint64_t space = 0;
    for (const auto &frame : set.frames())
    {
        space += frame->width() * frame->height();
    }
    float sq = std::sqrt(space);
    // Create rectangular sheet w/ extra padding
    int64_t z = (static_cast<int32_t>(sq) | (SpriteSheet::SIZE_ALIGNMENT - 1)) + 1;
    int64_t sx = ((z + z / SpriteSheet::SIZE_ADJUSTMENT) | (SpriteSheet::SIZE_ALIGNMENT * 2 - 1)) + 1;
    LOGI("space: %lu %lu ", space, sx);
    LOGI("sqrt: %f (%f) ", sq, sq * sq);
    return sx;
}

/**
 * @brief Places a frame in the best-fitting free rectangle, returning its index or NOT_FOUND.
 *         find sprite emplacement. enlarge the sheet size if needed
 * @param rects
 * @param frame
 * @param size
 * @return int
 */
int createPlacement(std::vector<Rect> &rects, CFrame &frame, Size &size)
{
    std::vector<Rect> newRects;
    bool found = false;
    // int i = 0;
    for (auto &rect : rects)
    {
        if (rect.x + rect.width == size.sx && rect.height >= frame.height())
        {
            int newMargin = frame.width() - rect.width;
            if (rect.y > 0)
            {
                Rect newRect{
                    .x = size.sx,
                    .y = 0,
                    .width = newMargin,
                    .height = rect.y,
                    .color = getRandomColor(),
                };
                newRects.push_back(newRect);
            }
            if (rect.y + rect.height < size.sy)
            {
                int y = rect.y + rect.height;
                Rect newRect{
                    .x = size.sx,
                    .y = y,
                    .width = newMargin,
                    .height = size.sy - y,
                    .color = getRandomColor(),
                };
                newRects.push_back(newRect);
            }

            // right aligned
            size.sx += frame.width() - rect.width;
            rect.width = frame.width();
            found = true;
            break; // return i;
        }
        else if (rect.y + rect.height == size.sy && rect.width >= frame.width())
        {
            int newMargin = frame.height() - rect.height;
            if (rect.x > 0)
            {
                Rect newRect{
                    .x = 0,
                    .y = size.sy,
                    .width = rect.x,
                    .height = newMargin,
                    .color = getRandomColor(),
                };
                newRects.push_back(newRect);
            };

            if (rect.x + rect.width < size.sx)
            {
                int x = rect.x + rect.width;
                Rect newRect{
                    .x = x,
                    .y = size.sy,
                    .width = size.sx - x,
                    .height = newMargin,
                    .color = getRandomColor(),
                };
                newRects.push_back(newRect);
            }

            // bottom aligned
            size.sy += frame.height() - rect.height;
            rect.height = frame.height();
            found = true;
            break;
        }
        //  ++i;
    }

    for (auto &rect : newRects)
        rects.push_back(rect);

    return !found ? SpriteSheet::NOT_FOUND : rects.size() - 1;
}

/**
 * @brief  merge neighbors rects
 *
 * @param rects
 */

void mergeRects(std::vector<Rect> &rects)
{
    std::set<int, std::greater<int>> merged_rects;
    auto isMerged = [&merged_rects](int i)
    {
        return merged_rects.find(i) != merged_rects.end();
    };

    bool merged;
    do
    {
        merged = false;
        for (size_t i = 0; i < rects.size(); ++i)
        {
            Rect &a = rects[i];
            if (isMerged(i))
                continue;
            for (size_t j = 0; j < rects.size(); ++j)
            {
                if (i == j || isMerged(j))
                    continue;
                Rect &b = rects[j];
                if ((a.height == b.height) &&
                    (a.y == b.y) &&
                    ((a.x == b.x + b.width) || (b.x == a.x + a.width)))
                {
                    Rect t{
                        .x = std::min(a.x, b.x),
                        .y = a.y,
                        .width = std::max(a.x + a.width, b.x + b.width) - std::min(a.x, b.x),
                        .height = a.height,
                        .color = a.color,
                    };
                    merged_rects.insert(j);
                    //                    printRect(a, "a");
                    //                  printRect(b, "b");
                    //                printRect(t, "t");
                    a = std::move(t);
                    merged = true;
                    break;
                }
                else if ((a.width == b.width) && (a.x == b.x) &&
                         ((a.y == b.y + b.height) || (b.y == a.y + a.height)))
                {
                    Rect t{
                        .x = a.x,
                        .y = std::min(a.y, b.y),
                        .width = a.width,
                        .height = std::max(a.y + a.height, b.y + b.height) - std::min(a.y, b.y),
                        .color = a.color,
                    };
                    merged_rects.insert(j);
                    //              printRect(a, "a");
                    //            printRect(b, "b");
                    //          printRect(t, "t");
                    a = std::move(t);
                    merged = true;
                }
            }
        }
    } while (merged);

    for (const auto &id : merged_rects)
    {
        rects.erase(rects.begin() + id);
    }
}

/**
 * @brief Subdivide a Rect after the image is inserted so that the extra
 *        space can be reuse for other images
 *
 * @param rects
 * @param i
 * @param f
 */
void subdivideRect(std::vector<Rect> &rects, const int i, const CFrame &f)
{
    std::vector<Rect> newRects;
    const Rect rect = rects[i];
    bool optFit = rect.height <= 2 * f.height();
    if (optFit)
    {
        const Rect newRect{
            .x = rect.x + f.width(),
            .y = rect.y,
            .width = rect.width - f.width(),
            .height = rect.height,
            .color = getRandomColor(),
        };
        newRects.emplace_back(newRect);

        if (f.height() < rect.height)
        {
            const Rect newRect{
                .x = rect.x,
                .y = rect.y + f.height(),
                .width = f.width(),
                .height = rect.height - f.height(),
                .color = getRandomColor(),
            };
            newRects.emplace_back(newRect);
        }
    }
    else
    {
        const Rect newRect{
            .x = rect.x + f.width(),
            .y = rect.y,
            .width = rect.width - f.width(),
            .height = f.height(),
            .color = getRandomColor(),
        };
        newRects.emplace_back(newRect);

        const Rect newRect2{
            .x = rect.x,
            .y = rect.y + f.height(),
            .width = rect.width,
            .height = rect.height - f.height(),
            .color = getRandomColor(),
        };
        newRects.emplace_back(newRect2);
    }
    rects.erase(rects.begin() + i);

    for (const auto &rect : newRects)
    {
        if (rect.height > 0 && rect.width > 0)
            rects.emplace_back(rect);
    }
}

/**
 * @brief Place all the sprites on the sheet.
 *
 * @param set frame set to be placed
 * @param sprites vector of sprites
 * @param size sheet size. will be increased as needed
 * @param rects rectangular areas that are free
 */
void placeSpritesOnSheet(CFrameSet &set, std::vector<Sprite> &sprites, const SpriteSheet::Flag flags, Size &size, std::vector<Rect> &rects)
{
    // Create wrapped frames with IDs
    const std::vector<CFrame *> &frames = set.frames();
    sprites.clear();
    sprites.reserve(frames.size());

    // create sprites with IDs
    for (size_t i = 0; i < frames.size(); ++i)
    {
        Sprite sprite{
            .x = SpriteSheet::NOT_FOUND,
            .y = SpriteSheet::NOT_FOUND,
            .width = frames[i]->width(),
            .height = frames[i]->height(),
            .frame = frames[i],
            .id = (int)i,
        };
        sprites.emplace_back(sprite);
    }

    // Create index view for sorting
    std::vector<size_t> indices(sprites.size());
    for (size_t i = 0; i < indices.size(); ++i)
        indices[i] = i;

    if (flags & SpriteSheet::sorted)
        std::sort(indices.begin(), indices.end(),
                  [&sprites](size_t a, size_t b)
                  { return sprites[a].height > sprites[b].height; });

    // add the original Rect emcompasing the
    // entire sheet
    rects.emplace_back(Rect{
        .x = 0,
        .y = 0,
        .width = size.sx,
        .height = size.sy,
        .color = getRandomColor(),
    });

    // list of Sprite not placed
    std::vector<int> notPlaced;
    notPlaced.reserve(frames.size());

    // loop through all the sprite to find them
    // a new position on the sheet
    for (size_t idx : indices)
    {
        Sprite &sprite = sprites[idx];
        CFrame &frame = *sprite.frame;
        int i = findBestMatchRect(rects, &frame);
        if (i != SpriteSheet::NOT_FOUND)
        {
            // update sprite coordonates
            const Rect rect = rects[i];
            sprite.x = rect.x;
            sprite.y = rect.y;
            subdivideRect(rects, i, frame);
        }
        else
        {
            if ((i = createPlacement(rects, frame, size)) != SpriteSheet::NOT_FOUND)
            {
                LOGI("==== fallback placement: %lux%lu %d", frame.width(), frame.height(), i);
                // update sprites coordonates
                const Rect rect = rects[i];
                sprite.x = rect.x;
                sprite.y = rect.y;
                subdivideRect(rects, i, frame);
            }
            else
            {
                LOGW("=== cannot find suitable rect for sprite %d [%d x %d]", sprite.id, frame.width(), frame.height());
                sprite.x = sprite.y = SpriteSheet::NOT_FOUND; // Already set, but explicit
                notPlaced.push_back(sprite.id);
                continue; // Skip mergeRects for unplaced sprites
            }
        }
        mergeRects(rects);
    }

    LOGI("==== FINAL");
    LOGI("total sprite: %lu [0x%.8x]", sprites.size(), sprites.size());
    LOGI("Not Placed: %lu", notPlaced.size());
    // printRects(rects);
}

/// @brief Create a file on disk to test the png output
/// @param png
/// @param filepath
/// @return
bool outputToFile(const std::vector<uint8_t> &png, const std::string_view &filepath)
{
    CFileWrap file;
    if (!file.open(filepath, "wb"))
    {
        LOGE("cannot create %s", filepath.data());
        return false;
    }

    if (file.write(png.data(), png.size()) != IFILE_OK)
    {
        LOGE("failed file %s", filepath.data());
        return false;
    }
    file.close();
    return true;
}

/**
 * @brief generate png metadata
 *
 * @param sprites
 * @param obltdata
 * @return true
 * @return false
 */
bool generateObtData(const std::vector<Sprite> &sprites, std::vector<uint8_t> &obltdata)
{
    constexpr uint32_t OBLT_VERSION = 2;
    constexpr uint32_t PNG_STANDARD_HDR_SIZE = 12;

    std::vector<size_t> indices(sprites.size());
    for (size_t i = 0; i < indices.size(); ++i)
        indices[i] = i;
    std::sort(indices.begin(), indices.end(),
              [&sprites](size_t a, size_t b)
              { return sprites[a].id < sprites[b].id; });

    const size_t totalSize = sizeof(CFrame::png_OBL5) +                         // hdr
                             sizeof(CFrame::oblv2DataUnit_t) * sprites.size() + // data
                             sizeof(uint32_t);                                  // crc32 placeholder
    obltdata.clear();
    obltdata.reserve(totalSize);

    // create block header
    CFrame::png_OBL5 hdr{
        .Length = (uint32_t)CFrame::toNet(totalSize - PNG_STANDARD_HDR_SIZE),
        .ChunkType = {'o', 'b', 'L', 'T'}, // "obLT",
        .Reserved = 0,
        .Version = OBLT_VERSION,
        .Count = (uint32_t)sprites.size(),
    };
    appendBytes(obltdata, reinterpret_cast<uint8_t *>(&hdr), sizeof(hdr));

    // append metadata
    for (size_t idx : indices)
    {
        const auto &sprite = sprites[idx];
        if (sprite.id != static_cast<int>(idx))
        {
            LOGE("Mismatch SpriteID: %d; expecting %lu", sprite.id, idx);
            return false;
        }
        CFrame::oblv2DataUnit_t meta{
            .x = (uint16_t)sprite.x,
            .y = (uint16_t)sprite.y,
            .sx = (uint16_t)sprite.frame->width(),
            .sy = (uint16_t)sprite.frame->height(),
        };
        appendBytes(obltdata, reinterpret_cast<uint8_t *>(&meta), sizeof(meta));
    }

    // add crc32 placeholder to be filled later
    uint32_t crc32Placeholder = 0;
    appendBytes(obltdata, reinterpret_cast<uint8_t *>(&crc32Placeholder), sizeof(crc32Placeholder));

    if (obltdata.size() != totalSize)
    {
        LOGE("unexpected error; obltdata size:%lu -- expected: %lu", obltdata.size(), totalSize);
        return false;
    }
    return true;
}

/**
 * @brief Calculate the upper bound for sprite sheet
 *
 * @param sprites
 * @return Size
 */
Size findFrameSetBounds(const std::vector<Sprite> &sprites)
{
    Size size{.sx = 0, .sy = 0};
    for (const auto &sprite : sprites)
    {
        if (sprite.x + sprite.frame->width() > size.sx)
            size.sx = sprite.x + sprite.frame->width();
        if (sprite.y + sprite.frame->height() > size.sy)
            size.sy = sprite.y + sprite.frame->height();
    }
    return size;
}

/**
 * @brief convert a frameset into a spritesheet stored as png data
 *
 * @param set
 * @param png
 * @return true
 * @return false
 */
bool toSpriteSheet(CFrameSet &set, std::vector<uint8_t> &png, const SpriteSheet::Flag flags)
{
    std::vector<Sprite> sprites;
    std::vector<Rect> rects;

    // Find a place for all the sprites
    int64_t sx = estimateSheetSize(set);
    if (sx > 4096)
    {
        LOGE("size is out of bound %d -- max allowed %d", sx, 4096);
        return false;
    }
    Size size{sx, sx};
    placeSpritesOnSheet(set, sprites, flags, size, rects);

    // Create Png Sheet
    // LOGI("size: %d %d", size.sx, size.sy);
    size = findFrameSetBounds(sprites);

    // LOGI("size: %d %d", size.sx, size.sy);
    CFrame sheet(size.sx, size.sy);
    // rects.clear();
    createSheet(sheet, sprites, rects);

    // generate oblt data
    std::vector<uint8_t> obltdata;
    if (!generateObtData(sprites, obltdata))
    {
        LOGE("failed to generate obltData");
        return false;
    }

    // sanity checks
    // checkOverlaps(sprites);
    // checkPixmap(sheet, sprites);

    // convert to png
    if (!sheet.toPng(png, obltdata))
    {
        LOGE("failed to generate png");
        return false;
    }

    return true;
}

int main(int argc, char *argv[], char *envp[])
{
    (void)envp;
    (void)argc;
    (void)argv;

    srand(time(nullptr));

    int imagesLimit = argc > 2 ? std::stoi(argv[2]) : 0;
    std::string_view in_filepath = argc > 1 ? argv[1] : "list.txt";
    CFrameSet set;

    getTestData(set, in_filepath);
    if (imagesLimit != 0)
    {
        LOGI("LIMIT: %d", imagesLimit);
        set.resize(imagesLimit);
    }

    std::vector<uint8_t> png;
    if (!toSpriteSheet(set, png, SpriteSheet::sorted))
    {
        LOGE("failed to build sprite sheet");
        return EXIT_FAILURE;
    }

    // output to file
    const char *out_filepath = "out/sheet.png";
    if (!outputToFile(png, out_filepath))
    {
        LOGE("failed to output png: %s", out_filepath);
        return EXIT_FAILURE;
    }

    CFrameSet set2;
    if (!extractFrameSet(set2, out_filepath))
    {
        LOGE("failed to extract: %s", out_filepath);
        return EXIT_FAILURE;
    }

    if (set2.getSize() != set.getSize())
    {
        LOGE("image count in spritesheet %ld; expecting %ld", set2.getSize(), set.getSize());
        return EXIT_FAILURE;
    }

    LOGI("sheet contains %lu sprites", set2.getSize());

    return EXIT_SUCCESS;
}
