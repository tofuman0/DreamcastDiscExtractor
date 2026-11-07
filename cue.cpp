#include "cue.hpp"

#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace DiscExtractor
{
Disc ReadCue(const std::filesystem::path& path)
{
    std::ifstream descriptor(path);
    if (!descriptor)
    {
        throw std::runtime_error("Failed to open CUE descriptor");
    }
    Disc disc;
    std::unordered_set<std::string> countedFiles;
    std::filesystem::path currentPath;
    Track track;
    bool hasTrack = false;
    bool hasIndex = false;
    std::string line;
    while (std::getline(descriptor, line))
    {
        std::istringstream record(line);
        std::string command;
        record >> command;
        for (char& character : command)
        {
            character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
        }
        if (command == "FILE")
        {
            std::string filename;
            std::string type;
            if (!(record >> std::quoted(filename) >> type) || type != "BINARY")
            {
                throw std::runtime_error("Unsupported CUE file record");
            }
            currentPath = path.parent_path() / std::filesystem::path(std::u8string(filename.begin(), filename.end()));
            std::error_code error;
            const uint64_t size = std::filesystem::file_size(currentPath, error);
            if (error)
            {
                throw std::runtime_error("Missing CUE track file: " + filename);
            }
            const auto canonical = std::filesystem::weakly_canonical(currentPath, error);
            if (error)
            {
                throw std::runtime_error("Invalid CUE track file: " + filename);
            }
            if (countedFiles.insert(canonical.generic_string()).second)
            {
                disc.m_Size += size;
            }
        }
        else if (command == "TRACK")
        {
            if (hasTrack && !hasIndex)
            {
                throw std::runtime_error("CUE track has no INDEX 01");
            }
            track = {};
            std::string mode;
            if (currentPath.empty() || !(record >> track.m_Number >> mode))
            {
                throw std::runtime_error("Invalid CUE track record");
            }
            track.m_Path = currentPath;
            track.m_Type = mode == "AUDIO" ? 0 : 4;
            if (mode == "MODE1/2048")
            {
                track.m_SectorSize = 2048;
            }
            else if (mode == "MODE1/2352" || mode == "MODE2/2352" || mode == "AUDIO")
            {
                track.m_SectorSize = 2352;
            }
            else
            {
                throw std::runtime_error("Unsupported CUE track mode: " + mode);
            }
            hasTrack = true;
            hasIndex = false;
        }
        else if (command == "INDEX" && hasTrack)
        {
            uint32_t number = 0;
            uint32_t minutes = 0;
            uint32_t seconds = 0;
            uint32_t frames = 0;
            char colon1 = 0;
            char colon2 = 0;
            if (!(record >> number >> minutes >> colon1 >> seconds >> colon2 >> frames) ||
                colon1 != ':' || colon2 != ':' || seconds >= 60 || frames >= 75)
            {
                throw std::runtime_error("Invalid CUE index");
            }
            if (number == 1)
            {
                if (hasIndex)
                {
                    throw std::runtime_error("Duplicate CUE INDEX 01");
                }
                const uint64_t index = (static_cast<uint64_t>(minutes) * 60 + seconds) * 75 + frames;
                track.m_Offset = index * track.m_SectorSize;
                if (track.m_Offset >= std::filesystem::file_size(track.m_Path))
                {
                    throw std::runtime_error("CUE index is outside the track file");
                }
                disc.m_Tracks.push_back(track);
                hasIndex = true;
            }
        }
    }
    if (disc.m_Tracks.empty() || (hasTrack && !hasIndex))
    {
        throw std::runtime_error("No indexed CUE tracks found");
    }
    return disc;
}

Disc ReadBin(const std::filesystem::path& path)
{
    std::error_code error;
    const uint64_t size = std::filesystem::file_size(path, error);
    if (error || size < 2048)
    {
        throw std::runtime_error("Missing or truncated BIN file");
    }
    Disc disc;
    disc.m_Size = size;
    disc.m_Tracks.push_back({1, 0, 4, 2048, path, 0});
    disc.m_Tracks.push_back({1, 0, 4, 2352, path, 0});
    return disc;
}
}
