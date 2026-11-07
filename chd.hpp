#pragma once

#include "gdi.hpp"

#include <array>
#include <cstdio>
#include <memory>
#include <vector>

struct _chd_file;

namespace DiscExtractor
{
struct FileCloser
{
    void operator()(std::FILE* file) const
    {
        if (file)
        {
            std::fclose(file);
        }
    }
};

class ChdReader
{
public:
    explicit ChdReader(const std::filesystem::path& path);
    ~ChdReader();
    ChdReader(const ChdReader&) = delete;
    ChdReader& operator=(const ChdReader&) = delete;
    Disc ReadTracks(const std::filesystem::path& path) const;
    std::array<uint8_t, 2448> ReadFrame(uint64_t frame);

private:
    std::unique_ptr<std::FILE, FileCloser> m_File;
    _chd_file* m_Chd = nullptr;
    std::vector<uint8_t> m_Buffer;
    uint32_t m_Hunk = UINT32_MAX;
};
}
