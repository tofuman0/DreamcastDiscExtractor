#include "gdi.hpp"

#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace DiscExtractor
{
Disc ReadGdi(const std::filesystem::path& path)
{
    std::ifstream descriptor(path);
    if (!descriptor)
    {
        throw std::runtime_error("Failed to open GDI descriptor");
    }

    uint32_t trackCount = 0;
    if (!(descriptor >> trackCount) || trackCount == 0 || trackCount > 99)
    {
        throw std::runtime_error("Invalid GDI track count");
    }

    std::string line;
    std::getline(descriptor, line);
    Disc disc;
    disc.m_Tracks.reserve(trackCount);
    std::unordered_set<std::string> countedFiles;
    uint32_t previousSector = 0;

    for (uint32_t index = 0; index < trackCount; index++)
    {
        if (!std::getline(descriptor, line))
        {
            throw std::runtime_error("Missing GDI track record");
        }

        std::istringstream record(line);
        Track track;
        std::string filename;
        std::string extra;
        if (!(record >> track.m_Number >> track.m_StartSector >> track.m_Type >> track.m_SectorSize
              >> std::quoted(filename) >> track.m_Offset) || (record >> extra))
        {
            throw std::runtime_error("Invalid GDI track record");
        }

        if (track.m_Number != index + 1 || (index > 0 && track.m_StartSector <= previousSector) ||
            (track.m_Type != 0 && track.m_Type != 4) ||
            (track.m_SectorSize != 2048 && track.m_SectorSize != 2352) || filename.empty())
        {
            throw std::runtime_error("Unsupported GDI track record");
        }

        previousSector = track.m_StartSector;
        track.m_Path = path.parent_path() / std::filesystem::path(std::u8string(filename.begin(), filename.end()));
        std::error_code error;
        const uint64_t fileSize = std::filesystem::file_size(track.m_Path, error);
        if (error || track.m_Offset >= fileSize || (fileSize - track.m_Offset) < track.m_SectorSize)
        {
            throw std::runtime_error("Missing or truncated GDI track: " + filename);
        }

        const std::string fileKey = std::filesystem::weakly_canonical(track.m_Path, error).generic_string();
        if (error)
        {
            throw std::runtime_error("Failed to resolve GDI track: " + filename);
        }
        if (countedFiles.insert(fileKey).second)
        {
            if (fileSize > std::numeric_limits<uint64_t>::max() - disc.m_Size)
            {
                throw std::runtime_error("GDI image is too large");
            }
            disc.m_Size += fileSize;
        }
        disc.m_Tracks.push_back(std::move(track));
    }

    while (std::getline(descriptor, line))
    {
        if (line.find_first_not_of(" \t\r") != std::string::npos)
        {
            throw std::runtime_error("Unexpected GDI track record");
        }
    }
    return disc;
}
}
