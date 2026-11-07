// SPDX-License-Identifier: MPL-2.0
// Based on OpenReliant src/formats/cdimage/cdi.zig.

#include "cdi.hpp"

#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace DiscExtractor
{
namespace
{
// DiscJuggler track layout reference: OpenReliant cdimage/cdi.zig (MPL-2.0).
class TableReader
{
public:
    explicit TableReader(std::vector<uint8_t> bytes) : m_Bytes(std::move(bytes))
    {
    }

    uint8_t Read8()
    {
        Require(1);
        return m_Bytes[m_Position++];
    }

    uint16_t Read16()
    {
        const uint16_t first = Read8();
        return first | static_cast<uint16_t>(Read8()) << 8;
    }

    uint32_t Read32()
    {
        const uint32_t first = Read16();
        return first | static_cast<uint32_t>(Read16()) << 16;
    }

    void Skip(size_t count)
    {
        Require(count);
        m_Position += count;
    }

private:
    void Require(size_t count) const
    {
        if (count > m_Bytes.size() - m_Position)
        {
            throw std::runtime_error("Truncated CDI descriptor");
        }
    }

    std::vector<uint8_t> m_Bytes;
    size_t m_Position = 0;
};

uint32_t ReadLittle32(const uint8_t* bytes)
{
    return static_cast<uint32_t>(bytes[0]) | (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) | (static_cast<uint32_t>(bytes[3]) << 24);
}
}

Disc ReadCdi(const std::filesystem::path& path)
{
    std::ifstream source(path, std::ios::binary | std::ios::ate);
    if (!source || source.tellg() < 8)
    {
        throw std::runtime_error("Missing or truncated CDI image");
    }
    const uint64_t size = static_cast<uint64_t>(source.tellg());
    source.seekg(-8, std::ios::end);
    std::array<uint8_t, 8> tail{};
    source.read(reinterpret_cast<char*>(tail.data()), tail.size());
    const uint32_t version = ReadLittle32(tail.data());
    const uint32_t tableLocation = ReadLittle32(tail.data() + 4);
    uint64_t start = tableLocation;
    if (version == 0x80000006)
    {
        if (tableLocation > size)
        {
            throw std::runtime_error("Invalid CDI descriptor offset");
        }
        start = size - tableLocation;
    }
    else if (version != 0x80000004 && version != 0x80000005)
    {
        throw std::runtime_error("Unsupported CDI version");
    }
    if (start == 0 || start >= size - 8 || size - 8 - start > 1024 * 1024)
    {
        throw std::runtime_error("Invalid CDI descriptor size");
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size - 8 - start));
    source.seekg(static_cast<std::streamoff>(start));
    source.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (!source)
    {
        throw std::runtime_error("Failed to read CDI descriptor");
    }
    TableReader table(std::move(bytes));
    const uint16_t sessions = table.Read16();
    if (sessions == 0 || sessions > 99)
    {
        throw std::runtime_error("Invalid CDI session count");
    }
    constexpr std::array<uint8_t, 20> TrackMarker =
    {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF
    };
    Disc disc;
    disc.m_Size = size;
    uint64_t offset = 0;
    for (uint16_t session = 0; session < sessions; session++)
    {
        const uint16_t count = table.Read16();
        if (count > 99 || disc.m_Tracks.size() + count > 99)
        {
            throw std::runtime_error("Invalid CDI track count");
        }
        for (uint16_t index = 0; index < count; index++)
        {
            if (table.Read32() != 0)
            {
                table.Skip(8);
            }
            for (uint8_t value : TrackMarker)
            {
                if (table.Read8() != value)
                {
                    throw std::runtime_error("Invalid CDI track marker");
                }
            }
            table.Skip(4);
            table.Skip(table.Read8());
            table.Skip(19);
            if (table.Read32() == 0x80000000)
            {
                table.Skip(8);
            }
            table.Skip(2);
            const uint32_t pregap = table.Read32();
            const uint32_t length = table.Read32();
            table.Skip(6);
            const uint32_t mode = table.Read32();
            table.Skip(12);
            const uint32_t firstSector = table.Read32();
            const uint32_t total = table.Read32();
            table.Skip(16);
            const uint32_t sectorCode = table.Read32();
            const std::array<uint32_t, 3> sizes = {2048, 2336, 2352};
            if (sectorCode >= sizes.size() || mode > 2 || total < pregap || length > total - pregap)
            {
                throw std::runtime_error("Invalid CDI track details");
            }
            const uint32_t sectorSize = sizes[sectorCode];
            const uint64_t trackBytes = static_cast<uint64_t>(total) * sectorSize;
            if (offset > start || trackBytes > start - offset)
            {
                throw std::runtime_error("CDI track extends beyond image data");
            }
            Track track;
            track.m_Number = static_cast<uint32_t>(disc.m_Tracks.size() + 1);
            track.m_StartSector = firstSector;
            track.m_Type = mode == 0 ? 0 : 4;
            track.m_SectorSize = sectorSize;
            track.m_Path = path;
            track.m_Offset = offset + static_cast<uint64_t>(pregap) * sectorSize;
            disc.m_Tracks.push_back(track);
            offset += trackBytes;
            table.Skip(29);
            if (version != 0x80000004)
            {
                table.Skip(5);
                if (table.Read32() == 0xFFFFFFFF)
                {
                    table.Skip(78);
                }
            }
        }
        table.Skip(12);
        if (version != 0x80000004)
        {
            table.Skip(1);
        }
    }
    if (disc.m_Tracks.empty())
    {
        throw std::runtime_error("No CDI tracks found");
    }
    return disc;
}
}
