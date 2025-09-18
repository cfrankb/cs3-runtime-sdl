#include <string>
#include <cstdint>

#pragma once

struct data_t
{
    uint8_t *ptr;
    size_t size;
};

class CAssetMan
{
public:
    static std::string &addTrailSlash(std::string &path);
    static void setPrefix(const std::string &prefix);
    static const std::string &getPrefix();
    static bool read(const std::string &filepath, data_t &data, bool terminator = false);
    static void free(const data_t data);

private:
    CAssetMan() {}
    ~CAssetMan() {}
};