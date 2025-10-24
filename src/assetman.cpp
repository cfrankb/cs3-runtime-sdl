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
#include "assetman.h"
#include "shared/FileWrap.h"
#include "logger.h"

#define DEFAULT_PREFIX "data/"

#if defined(__ANDROID__)
#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h> // For AAssetManager_fromJava()
#include <android/log.h>               // For logging
#include <string>
#include <vector>

// Global or class member to store the AAssetManager
AAssetManager *globalAssetManager = nullptr;

extern "C"
{
    JNIEXPORT void JNICALL Java_org_libsdl_app_SDLActivity_nativeInitAssetManager(JNIEnv *env, jobject /* this */, jobject java_asset_manager)
    {
        globalAssetManager = AAssetManager_fromJava(env, java_asset_manager);
        if (globalAssetManager == nullptr)
        {
            LOGE("Failed to get AAssetManager from Java");
        }
        else
        {
            LOGI("AAssetManager initialized successfully in C++");
        }
    }
} // extern "C"
#endif

namespace AssetMan
{
    std::string g_prefix = DEFAULT_PREFIX;

#if defined(__ANDROID__)
    static uint8_t *readBinary(const char *assetPath, size_t &size)
    {
        if (globalAssetManager == nullptr)
        {
            LOGE("AssetManager not initialized!");
            return nullptr;
        }

        AAsset *asset = AAssetManager_open(globalAssetManager, assetPath, AASSET_MODE_BUFFER);
        if (asset == nullptr)
        {
            LOGE("Failed to open asset: %s", assetPath);
            return nullptr;
        }

        off_t assetLength = AAsset_getLength(asset);
        size = assetLength;
        auto *buffer = new uint8_t[assetLength + 1];
        int bytesRead = AAsset_read(asset, buffer, assetLength);
        AAsset_close(asset);

        if (bytesRead < 0)
        {
            LOGE("Failed to read asset: %s", assetPath);
            return nullptr;
        }
        buffer[assetLength] = '\0'; // Null-terminate

        LOGI("Successfully read asset '%s', length: %lld, bytes read: %d", assetPath, (long long)assetLength, bytesRead);
        return buffer;
    }

#endif

    std::string defaultPrefix()
    {
        return DEFAULT_PREFIX;
    }

    std::string addTrailSlash(const std::string &path)
    {
        if (!path.empty() && path.back() != '/' &&
            path.back() != '\\')
        {
            return path + "/";
        }
        return path;
    }

    void setPrefix(const std::string &prefix)
    {
        g_prefix = addTrailSlash(prefix);
    }

    const std::string &getPrefix()
    {
        return g_prefix;
    }

    data_t read(const std::string &filepath, bool terminator)
    {
        data_t data;
#if defined(__ANDROID__)
        size_t size;
        uint8_t *ptr = readBinary(filepath.c_str(), size);
        if (!ptr)
        {
            LOGE("can't fetch asset: %s", filepath.c_str());
            return false;
        }
        data.assign(ptr, ptr + size);
        delete[] ptr;
        return true;
#else
        CFileWrap file;
        if (!file.open(filepath.c_str(), "rb"))
        {
            LOGE("can't open for reading: %s", filepath.c_str());
            return {};
        }
        size_t size = file.getSize();
        data.resize(terminator ? size + 1 : size);
        if (file.read(data.data(), size) != IFILE_OK)
        {
            LOGE("Failed to read data from %s", filepath.c_str());
            return {};
        }
        if (terminator)
            data[size] = '\0';
#endif
        return data;
    }
}