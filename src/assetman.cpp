#include "assetman.h"
#include "shared/FileWrap.h"
#include "logger.h"

#define DEFAULT_PREFIX "data/"

static std::string g_prefix = DEFAULT_PREFIX;

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
}

void CAssetMan::free(const data_t data)
{
    if (data.ptr)
        delete[] data.ptr;
}
