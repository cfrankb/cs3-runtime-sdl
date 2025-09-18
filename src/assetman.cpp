#include "assetman.h"
#include "shared/FileWrap.h"
#include "logger.h"

#define DEFAULT_PREFIX "data/"

#ifdef __ANDROID__
#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h> // For AAssetManager_fromJava()
#include <android/log.h> // For logging
#include <string>
#include <vector>

#define LOG_TAG "MyNativeAssets"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global or class member to store the AAssetManager
AAssetManager* globalAssetManager = nullptr;

extern "C" {
    JNIEXPORT void JNICALL Java_org_libsdl_app_SDLActivity_nativeInitAssetManager (JNIEnv *env, jobject /* this */, jobject java_asset_manager)
    {
        globalAssetManager = AAssetManager_fromJava(env, java_asset_manager);
        if (globalAssetManager == nullptr) {
            LOGE("Failed to get AAssetManager from Java");
        } else {
            LOGI("AAssetManager initialized successfully in C++");
        }
    }
} // extern "C"

uint8_t *readBinary(const char *assetPath, size_t & size)
{
    if (globalAssetManager == nullptr) {
        LOGE("AssetManager not initialized!");
        return nullptr;
    }

    AAsset* asset = AAssetManager_open(globalAssetManager, assetPath, AASSET_MODE_BUFFER);
    if (asset == nullptr) {
        LOGE("Failed to open asset: %s", assetPath);
        return nullptr;
    }

    off_t assetLength = AAsset_getLength(asset);
    size = assetLength;
    uint8_t *buffer = new uint8_t[assetLength + 1];
    int bytesRead = AAsset_read(asset, buffer, assetLength);
    AAsset_close(asset);

    if (bytesRead < 0) {
        LOGE("Failed to read asset: %s", assetPath);
        return nullptr;
    }
    buffer[assetLength] = '\0'; // Null-terminate

    LOGI("Successfully read asset '%s', length: %lld, bytes read: %d", assetPath, (long long)assetLength, bytesRead);
    return buffer;
}

#endif



static std::string g_prefix = DEFAULT_PREFIX;


const std::string defaultPrefix()
{
    return DEFAULT_PREFIX;
}

std::string &CAssetMan::addTrailSlash(std::string &path)
{
    if (path.size() != 0 && path.back() != '/' &&
        path.back() != '\\')
    {
        path += "/";
    }
    return path;
}

void CAssetMan::setPrefix(const std::string &prefix)
{
    std::string tmp = prefix;
    addTrailSlash(tmp);
    g_prefix = tmp;
}

const std::string &CAssetMan::getPrefix()
{
    return g_prefix;
}

bool CAssetMan::read(const std::string &filepath, data_t &data, bool terminator)
{
#ifdef __ANDROID__
    data.ptr = readBinary(filepath.c_str(), data.size);
    if (!data.ptr)
    {
        ELOG("can't fetch asset: %s\n", filepath.c_str());
        return false;
    }
    return true;
#else
    CFileWrap file;
    if (file.open(filepath.c_str(), "rb"))
    {
        data.size = file.getSize();
        data.ptr = new uint8_t[terminator ? data.size + 1 : data.size];
        if (terminator)
            data.ptr[data.size] = '\0';
        file.read(data.ptr, data.size);
        return true;
    }
    else
    {
        ELOG("can't read: %s\n", filepath.c_str());
        return false;
    }
#endif
}

void CAssetMan::free(const data_t data)
{
    if (data.ptr)
        delete[] data.ptr;
}

