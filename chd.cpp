#include "chd.hpp"

#include <libchdr/chd.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

namespace DiscExtractor
{
ChdReader::ChdReader(const std::filesystem::path& path)
{
#ifdef _WIN32
    m_File.reset(_wfopen(path.c_str(), L"rb"));
#else
    m_File.reset(std::fopen(path.c_str(), "rb"));
#endif
    if (!m_File || chd_open_file(m_File.get(), CHD_OPEN_READ, nullptr, &m_Chd) != CHDERR_NONE)
    {
        throw std::runtime_error("Failed to open CHD image");
    }
    const chd_header* header = chd_get_header(m_Chd);
    if (!header || header->hunkbytes == 0 || header->hunkbytes % 2448 != 0 || header->hunkbytes > 64 * 1024 * 1024)
    {
        chd_close(m_Chd);
        m_Chd = nullptr;
        throw std::runtime_error("Unsupported CHD sector layout");
    }
    m_Buffer.resize(header->hunkbytes);
}

ChdReader::~ChdReader()
{
    if (m_Chd)
    {
        chd_close(m_Chd);
    }
}

Disc ChdReader::ReadTracks(const std::filesystem::path& path) const
{
    Disc disc;
    disc.m_Size = std::filesystem::file_size(path);
    uint64_t frameOffset = 0;
    uint32_t discSector = 0;
    for (uint32_t index = 0; index < 99; index++)
    {
        std::array<char, 512> metadata{};
        uint32_t tag = GDROM_TRACK_METADATA_TAG;
        chd_error result = chd_get_metadata(m_Chd, tag, index, metadata.data(), metadata.size(), nullptr, nullptr, nullptr);
        if (result != CHDERR_NONE)
        {
            tag = GDROM_OLD_METADATA_TAG;
            result = chd_get_metadata(m_Chd, tag, index, metadata.data(), metadata.size(), nullptr, nullptr, nullptr);
        }
        if (result != CHDERR_NONE)
        {
            tag = CDROM_TRACK_METADATA2_TAG;
            result = chd_get_metadata(m_Chd, tag, index, metadata.data(), metadata.size(), nullptr, nullptr, nullptr);
        }
        if (result != CHDERR_NONE)
        {
            tag = CDROM_TRACK_METADATA_TAG;
            result = chd_get_metadata(m_Chd, tag, index, metadata.data(), metadata.size(), nullptr, nullptr, nullptr);
        }
        if (result != CHDERR_NONE)
        {
            break;
        }
        int32_t number = 0;
        int32_t frames = 0;
        int32_t padding = 0;
        int32_t pregap = 0;
        int32_t postgap = 0;
        std::array<char, 32> type{};
        std::array<char, 32> subtype{};
        std::array<char, 32> pregapType{};
        std::array<char, 32> pregapSubtype{};
        int32_t fields = 0;
        if (tag == GDROM_TRACK_METADATA_TAG || tag == GDROM_OLD_METADATA_TAG)
        {
            fields = std::sscanf(metadata.data(), "TRACK:%d TYPE:%31s SUBTYPE:%31s FRAMES:%d PAD:%d PREGAP:%d PGTYPE:%31s PGSUB:%31s POSTGAP:%d",
                                 &number, type.data(), subtype.data(), &frames, &padding, &pregap, pregapType.data(), pregapSubtype.data(), &postgap);
            if (fields != 9)
            {
                throw std::runtime_error("Invalid CHD GD-ROM metadata");
            }
        }
        else if (tag == CDROM_TRACK_METADATA2_TAG)
        {
            fields = std::sscanf(metadata.data(), "TRACK:%d TYPE:%31s SUBTYPE:%31s FRAMES:%d PREGAP:%d PGTYPE:%31s PGSUB:%31s POSTGAP:%d",
                                 &number, type.data(), subtype.data(), &frames, &pregap, pregapType.data(), pregapSubtype.data(), &postgap);
            if (fields != 8)
            {
                throw std::runtime_error("Invalid CHD CD-ROM metadata");
            }
        }
        else
        {
            fields = std::sscanf(metadata.data(), "TRACK:%d TYPE:%31s SUBTYPE:%31s FRAMES:%d",
                                 &number, type.data(), subtype.data(), &frames);
            if (fields != 4)
            {
                throw std::runtime_error("Invalid CHD track metadata");
            }
        }
        if (number != static_cast<int32_t>(index + 1) || frames <= 0 || padding < 0 || pregap < 0 || postgap < 0)
        {
            throw std::runtime_error("Invalid CHD track details");
        }
        const bool pregapStored = pregap > 0 && pregapType[0] == 'V';
        if (pregapStored && pregap > frames)
        {
            throw std::runtime_error("Invalid CHD pregap");
        }
        const std::string trackType(type.data());
        Track track;
        track.m_Number = index + 1;
        track.m_Type = trackType == "AUDIO" ? 0 : 4;
        if (trackType == "MODE1" || trackType == "MODE2_FORM1")
        {
            track.m_SectorSize = 2048;
        }
        else if (trackType == "MODE1_RAW" || trackType == "MODE2_RAW" || trackType == "AUDIO")
        {
            track.m_SectorSize = 2352;
        }
        else
        {
            throw std::runtime_error("Unsupported CHD track type: " + trackType);
        }
        if ((tag == GDROM_TRACK_METADATA_TAG || tag == GDROM_OLD_METADATA_TAG) && index == 2)
        {
            discSector = 45000;
        }
        track.m_StartSector = discSector + static_cast<uint32_t>(pregap);
        track.m_Path = path;
        track.m_Offset = frameOffset + (pregapStored ? static_cast<uint32_t>(pregap) : 0);
        track.m_Compressed = true;
        disc.m_Tracks.push_back(track);
        discSector = track.m_StartSector + static_cast<uint32_t>(frames - (pregapStored ? pregap : 0));
        frameOffset += (static_cast<uint64_t>(frames) + 3) & ~uint64_t(3);
    }
    if (disc.m_Tracks.empty())
    {
        throw std::runtime_error("No CHD CD-ROM tracks found");
    }
    return disc;
}

std::array<uint8_t, 2448> ChdReader::ReadFrame(uint64_t frame)
{
    const uint64_t byteOffset = frame * 2448;
    const uint64_t hunk = byteOffset / m_Buffer.size();
    if (hunk >= chd_get_header(m_Chd)->totalhunks || hunk > std::numeric_limits<uint32_t>::max())
    {
        throw std::runtime_error("CHD frame is outside the image");
    }
    if (m_Hunk != hunk)
    {
        if (chd_read(m_Chd, static_cast<uint32_t>(hunk), m_Buffer.data()) != CHDERR_NONE)
        {
            throw std::runtime_error("Failed to decode CHD data");
        }
        m_Hunk = static_cast<uint32_t>(hunk);
    }
    std::array<uint8_t, 2448> result{};
    const size_t offset = static_cast<size_t>(byteOffset % m_Buffer.size());
    std::copy_n(m_Buffer.data() + offset, result.size(), result.data());
    return result;
}
}
