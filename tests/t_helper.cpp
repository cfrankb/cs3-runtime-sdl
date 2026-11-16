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

#include <cstdio>
#include <cstring>
#include <list>
#include <set>
#include <string>
#include <vector>
#include <zlib.h>
#include "../src/logger.h"
#include "../src/shared/helper.h"
#include "t_helper.h"

namespace
{
std::vector<uint8_t> makePattern(size_t n, uint8_t seed)
{
    std::vector<uint8_t> data(n);
    for (size_t i = 0; i < n; ++i)
    {
        data[i] = static_cast<uint8_t>((i * 31 + seed) & 0xff);
    }
    return data;
}

bool writeFile(const std::string &path, const std::vector<uint8_t> &data)
{
    FILE *f = fopen(path.c_str(), "wb");
    if (!f)
    {
        LOGE("helper: cannot create %s", path.c_str());
        return false;
    }
    const size_t written = fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    return written == data.size();
}
} // namespace

bool test_helper_files()
{
    const std::string src = "tests/out/helper_src.bin";
    const std::string dst = "tests/out/helper_dst.bin";
    const std::string partA = "tests/out/helper_a.bin";
    const std::string partB = "tests/out/helper_b.bin";
    const std::string joined = "tests/out/helper_join.bin";

    // copyFile round-trip: byte identical
    const std::vector<uint8_t> payload = makePattern(1000, 7);
    if (!writeFile(src, payload))
    {
        return false;
    }
    std::string errMsg;
    if (!copyFile(src, dst, errMsg) || !errMsg.empty())
    {
        LOGE("copyFile failed: '%s'", errMsg.c_str());
        return false;
    }
    const std::vector<uint8_t> copied = readFile(dst.c_str());
    if (copied != payload)
    {
        LOGE("copyFile produced %zu bytes; expecting %zu identical",
             copied.size(), payload.size());
        return false;
    }

    // copyFile missing source: failure with non-empty message
    errMsg.clear();
    if (copyFile("tests/out/helper_does_not_exist.bin", dst, errMsg) ||
        errMsg.empty())
    {
        LOGE("copyFile missing source: result/msg wrong ('%s')", errMsg.c_str());
        return false;
    }

    // concat of two files equals the concatenation
    const std::vector<uint8_t> a = makePattern(300, 1);
    const std::vector<uint8_t> b = makePattern(400, 2);
    if (!writeFile(partA, a) || !writeFile(partB, b))
    {
        return false;
    }
    std::list<std::string> parts = {partA, partB};
    errMsg.clear();
    if (!concat(parts, joined, errMsg) || !errMsg.empty())
    {
        LOGE("concat failed: '%s'", errMsg.c_str());
        return false;
    }
    std::vector<uint8_t> expected = a;
    expected.insert(expected.end(), b.begin(), b.end());
    if (readFile(joined.c_str()) != expected)
    {
        LOGE("concat output mismatch");
        return false;
    }

    // concat with a missing member: failure with non-empty message
    parts.push_back("tests/out/helper_missing.bin");
    errMsg.clear();
    if (concat(parts, joined, errMsg) || errMsg.empty())
    {
        LOGE("concat missing member: result/msg wrong ('%s')", errMsg.c_str());
        return false;
    }

    // readFile hit and miss
    const std::vector<uint8_t> read = readFile(src.c_str());
    if (read.size() != 1000 || read[0] != payload[0] || read[999] != payload[999])
    {
        LOGE("readFile hit mismatch");
        return false;
    }
    if (!readFile("tests/out/helper_nope.bin").empty())
    {
        LOGE("readFile miss is not empty");
        return false;
    }
    return true;
}

bool test_helper_compress()
{
    const std::vector<uint8_t> plain = makePattern(4096, 3);

    // vector<uint8_t> overload round-trip through uncompress
    std::vector<uint8_t> packed;
    const int rc8 = compressData(plain, packed);
    if (rc8 != Z_OK)
    {
        LOGE("compressData(u8) return %d", rc8);
        return false;
    }
    std::vector<uint8_t> restored(plain.size());
    unsigned long restoredSize = restored.size();
    if (uncompress(restored.data(), &restoredSize, packed.data(), packed.size()) != Z_OK ||
        restoredSize != plain.size() || restored != plain)
    {
        LOGE("u8 decompress round-trip mismatch");
        return false;
    }

    // vector<uint16_t> overload round-trip
    std::vector<uint16_t> plain16(2048);
    for (size_t i = 0; i < plain16.size(); ++i)
    {
        plain16[i] = static_cast<uint16_t>(i * 131);
    }
    std::vector<uint8_t> packed16;
    if (compressData(plain16, packed16) != Z_OK)
    {
        LOGE("compressData(u16) failed");
        return false;
    }
    std::vector<uint8_t> raw16(plain16.size() * sizeof(uint16_t));
    restoredSize = raw16.size();
    if (uncompress(raw16.data(), &restoredSize, packed16.data(), packed16.size()) != Z_OK ||
        restoredSize != raw16.size() ||
        memcmp(raw16.data(), plain16.data(), raw16.size()) != 0)
    {
        LOGE("u16 decompress round-trip mismatch");
        return false;
    }

    // raw-pointer overload: caller frees per header contract
    std::vector<unsigned char> in(plain.begin(), plain.end());
    unsigned char *out = nullptr;
    unsigned long outSize = 0;
    const int rc = compressData(in.data(), in.size(), &out, outSize);
    bool ok = rc == Z_OK && out != nullptr && outSize > 0;
    if (ok)
    {
        std::vector<uint8_t> restoredRaw(in.size());
        restoredSize = restoredRaw.size();
        ok = uncompress(restoredRaw.data(), &restoredSize, out, outSize) == Z_OK &&
             restoredSize == in.size() &&
             memcmp(restoredRaw.data(), in.data(), in.size()) == 0;
    }
    delete[] out;
    if (!ok)
    {
        LOGE("raw compressData round-trip mismatch (rc %d)", rc);
        return false;
    }
    return true;
}

bool test_helper_strings()
{
    char buf[] = "aB3 z9-!";
    const char *ret = toUpper(buf);
    if (ret != buf || std::string(buf) != "AB3 Z9-!")
    {
        LOGE("toUpper produced '%s'", buf);
        return false;
    }

    struct
    {
        int in;
        int out;
    } table[] = {
        {'a', 'A'},
        {'z', 'Z'},
        {'M', 'M'},
        {'5', '5'},
        {' ', '_'},
        {'-', '_'},
        {'\t', '_'},
    };
    for (const auto &row : table)
    {
        if (upperClean(row.in) != row.out)
        {
            LOGE("upperClean(%d) = %d; expecting %d", row.in,
                 upperClean(row.in), row.out);
            return false;
        }
    }
    return true;
}

bool test_helper_uuid()
{
    const std::string uuid = getUUID();
    if (uuid.size() != 36 || uuid[8] != '-' || uuid[13] != '-' ||
        uuid[18] != '-' || uuid[23] != '-')
    {
        LOGE("uuid '%s' is not 8-4-4-4-12", uuid.c_str());
        return false;
    }
    const auto isHex = [](char c)
    { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); };
    for (size_t i = 0; i < uuid.size(); ++i)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            continue;
        }
        if (!isHex(uuid[i]))
        {
            LOGE("uuid '%s' has non-hex at %zu", uuid.c_str(), i);
            return false;
        }
    }
    if (uuid[14] != '4' || std::string("89ab").find(uuid[19]) == std::string::npos)
    {
        LOGE("uuid '%s' is not v4/RFC-4122", uuid.c_str());
        return false;
    }

    std::set<std::string> seen;
    for (int i = 0; i < 100; ++i)
    {
        seen.insert(getUUID());
    }
    if (seen.size() != 100)
    {
        LOGE("100 uuids produced %zu distinct", seen.size());
        return false;
    }
    return true;
}
