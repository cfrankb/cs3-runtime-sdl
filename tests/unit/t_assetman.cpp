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

#include <filesystem>

#include "../../src/logger.h"
#include "../../src/assetman.h"
#include "t_assetman.h"

bool test_assetman()
{
    // prefix handling
    if (AssetMan::defaultPrefix() != "data/")
    {
        LOGE("defaultPrefix: '%s'; expecting 'data/'", AssetMan::defaultPrefix().c_str());
        return false;
    }
    if (AssetMan::addTrailSlash("foo") != "foo/" ||
        AssetMan::addTrailSlash("foo/") != "foo/" ||
        AssetMan::addTrailSlash("foo\\") != "foo\\" ||
        !AssetMan::addTrailSlash("").empty())
    {
        LOGE("addTrailSlash mismatch");
        return false;
    }
    AssetMan::setPrefix("tests/in");
    if (AssetMan::getPrefix() != "tests/in/")
    {
        LOGE("getPrefix: '%s'; expecting 'tests/in/'", AssetMan::getPrefix().c_str());
        return false;
    }
    AssetMan::setPrefix("tests/in/");
    if (AssetMan::getPrefix() != "tests/in/")
    {
        LOGE("setPrefix doubled the slash");
        return false;
    }

    // binary read of a repo fixture
    const std::string path = "tests/in/test.rec";
    const auto expected = std::filesystem::file_size(path);
    const data_t raw = AssetMan::read(path);
    if (raw.size() != expected)
    {
        LOGE("read: size %zu; expecting %zu", raw.size(), expected);
        return false;
    }
    const data_t terminated = AssetMan::read(path, true);
    if (terminated.size() != expected + 1 || terminated.back() != '\0' ||
        terminated.front() != raw.front())
    {
        LOGE("read with terminator mismatch");
        return false;
    }

    // missing file yields empty data, no crash
    if (!AssetMan::read("tests/in/does-not-exist.dat").empty())
    {
        LOGE("missing file returned data");
        return false;
    }

    AssetMan::setPrefix(AssetMan::defaultPrefix()); // restore
    return true;
}
