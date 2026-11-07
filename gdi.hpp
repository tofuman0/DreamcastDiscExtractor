#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace DiscExtractor
{
struct Track
{
    uint32_t m_Number = 0;
    uint32_t m_StartSector = 0;
    uint32_t m_Type = 0;
    uint32_t m_SectorSize = 0;
    std::filesystem::path m_Path;
    uint64_t m_Offset = 0;
    bool m_Compressed = false;
};

struct Disc
{
    std::vector<Track> m_Tracks;
    uint64_t m_Size = 0;
};

// Read a GDI descriptor without opening or modifying its track files for writing.
Disc ReadGdi(const std::filesystem::path& path);

}
