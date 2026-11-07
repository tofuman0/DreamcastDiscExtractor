#include "image.hpp"
#include "cue.hpp"
#include "chd.hpp"
#include "cdi.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <limits>
#include <memory>
#include <functional>
#include <set>
#include <stdexcept>

namespace DiscExtractor
{
namespace
{
constexpr size_t SectorSize = 2048;
using Sector = std::array<uint8_t, SectorSize>;

uint32_t Read32(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

class SectorReader
{
public:
    explicit SectorReader(const Track& track) : m_Track(track)
    {
        if (track.m_Compressed)
        {
            m_Chd = std::make_unique<ChdReader>(track.m_Path);
        }
        else
        {
            m_Source.open(track.m_Path, std::ios::binary);
            if (!m_Source)
            {
                throw std::runtime_error("Failed to open data track");
            }
        }
    }

    Sector Read(uint32_t sector)
    {
        std::array<uint8_t, 2352> raw{};
        if (m_Chd)
        {
            const auto frame = m_Chd->ReadFrame(m_Track.m_Offset + sector);
            std::copy_n(frame.begin(), raw.size(), raw.begin());
        }
        else
        {
            const uint64_t offset = m_Track.m_Offset + static_cast<uint64_t>(sector) * m_Track.m_SectorSize;
            if (offset != m_NextOffset)
            {
                m_Source.clear();
                m_Source.seekg(static_cast<std::streamoff>(offset));
            }
            m_Source.read(reinterpret_cast<char*>(raw.data()), m_Track.m_SectorSize);
            if (!m_Source)
            {
                throw std::runtime_error("Disc image is truncated");
            }
            m_NextOffset = offset + m_Track.m_SectorSize;
        }
        size_t offset = 0;
        if (m_Track.m_SectorSize == 2352)
        {
            const bool hasSync = raw[0] == 0 && raw[11] == 0 &&
                std::all_of(raw.begin() + 1, raw.begin() + 11, [](uint8_t value)
                {
                    return value == 0xFF;
                });
            if (!hasSync || (raw[15] != 1 && raw[15] != 2) ||
                (raw[15] == 2 && ((raw[18] & 0x20) != 0 ||
                                  !std::equal(raw.begin() + 16, raw.begin() + 20, raw.begin() + 20))))
            {
                throw std::runtime_error("Invalid data sector");
            }
            offset = raw[15] == 2 ? 24 : 16;
        }
        else if (m_Track.m_SectorSize == 2336)
        {
            if ((raw[2] & 0x20) != 0 || !std::equal(raw.begin(), raw.begin() + 4, raw.begin() + 4))
            {
                throw std::runtime_error("Invalid Mode 2 data sector");
            }
            offset = 8;
        }
        Sector result{};
        std::copy_n(raw.begin() + offset, result.size(), result.begin());
        return result;
    }

private:
    Track m_Track;
    std::ifstream m_Source;
    uint64_t m_NextOffset = std::numeric_limits<uint64_t>::max();
    std::unique_ptr<ChdReader> m_Chd;
};

std::string ReadText(const Sector& sector, size_t start, size_t length)
{
    std::string text(reinterpret_cast<const char*>(sector.data() + start), length);
    const size_t last = text.find_last_not_of(" \0", std::string::npos, 2);
    if (last == std::string::npos)
    {
        return {};
    }
    text.resize(last + 1);
    return text;
}

struct Record
{
    uint32_t m_Sector = 0;
    uint32_t m_Size = 0;
    bool m_Directory = false;
    std::string m_Name;
};

Record ReadRecord(const uint8_t* data, size_t available)
{
    if (available < 34 || data[0] < 34 || data[0] > available || 33 + data[32] > data[0])
    {
        throw std::runtime_error("Invalid ISO directory record");
    }
    if ((data[25] & 0x80) != 0)
    {
        throw std::runtime_error("Multi-extent files are not supported");
    }
    Record result;
    result.m_Sector = Read32(data + 2);
    result.m_Size = Read32(data + 10);
    result.m_Directory = (data[25] & 2) != 0;
    result.m_Name.assign(reinterpret_cast<const char*>(data + 33), data[32]);
    return result;
}

std::vector<File> ReadFiles(SectorReader& source, const Record& root, uint32_t base)
{
    std::vector<File> files;
    std::set<uint32_t> directories;
    std::function<void(const Record&, const std::filesystem::path&, uint32_t)> visit;
    visit = [&](const Record& directory, const std::filesystem::path& parent, uint32_t depth)
    {
        if (depth > 128 || directory.m_Sector < base || directory.m_Size > 64 * 1024 * 1024 ||
            !directories.insert(directory.m_Sector).second || directories.size() > 100000)
        {
            throw std::runtime_error("Invalid ISO directory structure");
        }
        const uint32_t count = (directory.m_Size + SectorSize - 1) / SectorSize;
        for (uint32_t index = 0; index < count; index++)
        {
            const Sector sector = source.Read(directory.m_Sector - base + index);
            const size_t limit = std::min<size_t>(SectorSize, directory.m_Size - static_cast<size_t>(index) * SectorSize);
            size_t pos = 0;
            while (pos < limit && sector[pos] != 0)
            {
                const Record record = ReadRecord(sector.data() + pos, limit - pos);
                pos += sector[pos];
                if (record.m_Name == std::string(1, '\0') || record.m_Name == std::string(1, '\1'))
                {
                    continue;
                }
                std::string name = record.m_Name;
                const auto version = name.find(';');
                if (version != std::string::npos)
                {
                    name.resize(version);
                }
                if (!name.empty() && name.back() == '.')
                {
                    name.pop_back();
                }
                if (name.empty() || name == "." || name == ".." || name.find_first_of("/\\:") != std::string::npos)
                {
                    throw std::runtime_error("Unsafe filename in disc image");
                }
                const auto path = parent / std::filesystem::path(name);
                if (record.m_Directory)
                {
                    visit(record, path, depth + 1);
                }
                else
                {
                    if (record.m_Sector < base)
                    {
                        throw std::runtime_error("Invalid file sector");
                    }
                    files.push_back({path, record.m_Sector - base, record.m_Size});
                }
            }
        }
    };
    visit(root, {}, 0);
    return files;
}
}

Image::Image(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character)
    {
        return static_cast<char>(std::tolower(character));
    });
    if (extension == ".gdi")
    {
        m_Disc = ReadGdi(path);
    }
    else if (extension == ".cue")
    {
        m_Disc = ReadCue(path);
    }
    else if (extension == ".cdi")
    {
        m_Disc = ReadCdi(path);
    }
    else if (extension == ".chd")
    {
        ChdReader source(path);
        m_Disc = source.ReadTracks(path);
    }
    else if (extension == ".bin")
    {
        m_Disc = ReadBin(path);
    }
    else
    {
        throw std::runtime_error("Supported formats: GDI, CUE, BIN, CHD and CDI");
    }
    for (auto it = m_Disc.m_Tracks.rbegin(); it != m_Disc.m_Tracks.rend(); ++it)
    {
        if (it->m_Type != 4)
        {
            continue;
        }
        try
        {
            SectorReader source(*it);
            const Sector pvd = source.Read(16);
            if (pvd[0] != 1 || std::string(reinterpret_cast<const char*>(pvd.data() + 1), 5) != "CD001")
            {
                continue;
            }
            const Record root = ReadRecord(pvd.data() + 156, SectorSize - 156);
            try
            {
                m_Files = ReadFiles(source, root, it->m_StartSector);
                m_BaseSector = it->m_StartSector;
            }
            catch (const std::runtime_error&)
            {
                m_Files = ReadFiles(source, root, 0);
            }
            m_DataTrack = *it;
            const Sector boot = source.Read(0);
            if (std::string(reinterpret_cast<const char*>(boot.data()), 15) == "SEGA SEGAKATANA")
            {
                m_Details.m_Title = ReadText(boot, 0x80, 128);
                m_Details.m_Region = ReadText(boot, 0x30, 8);
                m_Details.m_GameId = ReadText(boot, 0x40, 10);
                m_Details.m_ReleaseDate = ReadText(boot, 0x50, 8);
                m_Details.m_Publisher = ReadText(boot, 0x10, 16);
            }
            if (extension == ".bin")
            {
                m_Disc.m_Tracks = {m_DataTrack};
            }
            return;
        }
        catch (const std::runtime_error&)
        {
            continue;
        }
    }
    throw std::runtime_error("No readable ISO filesystem found in the image");
}

const Details& Image::GetDetails() const
{
    return m_Details;
}

const std::vector<File>& Image::GetFiles() const
{
    return m_Files;
}

uint64_t Image::GetSize() const
{
    return m_Disc.m_Size;
}

size_t Image::GetTrackCount() const
{
    return m_Disc.m_Tracks.size();
}

void Image::Extract(const std::filesystem::path& output, const std::atomic_bool& cancelled,
                    const std::function<void(const Progress&)>& progress) const
{
    const auto root = std::filesystem::weakly_canonical(output);
    if (!std::filesystem::is_directory(root))
    {
        throw std::runtime_error("Output folder does not exist");
    }
    Progress state;
    state.m_FileCount = m_Files.size();
    for (const File& file : m_Files)
    {
        state.m_TotalSize += file.m_Size;
        const auto target = root / file.m_Path;
        if (std::filesystem::exists(target))
        {
            throw std::runtime_error("Output file already exists: " + file.m_Path.string());
        }
        const auto resolved = std::filesystem::weakly_canonical(target);
        const auto relative = resolved.lexically_relative(root);
        if (relative.empty() || *relative.begin() == "..")
        {
            throw std::runtime_error("Output path escapes the selected folder");
        }
    }
    SectorReader source(m_DataTrack);
    for (const File& file : m_Files)
    {
        if (cancelled)
        {
            throw std::runtime_error("Extraction cancelled");
        }
        const auto target = root / file.m_Path;
        std::filesystem::create_directories(target.parent_path());
        state.m_File = file.m_Path.string();
        state.m_FileBytes = 0;
        state.m_FileSize = file.m_Size;
        state.m_FileIndex++;
        progress(state);
        std::FILE* rawFile = nullptr;
#ifdef _WIN32
        rawFile = _wfopen(target.c_str(), L"wbx");
#else
        rawFile = std::fopen(target.c_str(), "wbx");
#endif
        if (!rawFile)
        {
            throw std::runtime_error("Failed to create output file: " + file.m_Path.string());
        }
        try
        {
            std::unique_ptr<std::FILE, FileCloser> destination(rawFile);
            while (state.m_FileBytes < state.m_FileSize)
            {
                if (cancelled)
                {
                    throw std::runtime_error("Extraction cancelled");
                }
                const auto sector = source.Read(file.m_Sector + static_cast<uint32_t>(state.m_FileBytes / SectorSize));
                const size_t bytes = static_cast<size_t>(std::min<uint64_t>(SectorSize, state.m_FileSize - state.m_FileBytes));
                if (std::fwrite(sector.data(), 1, bytes, destination.get()) != bytes)
                {
                    throw std::runtime_error("Failed to write output file");
                }
                state.m_FileBytes += bytes;
                state.m_TotalBytes += bytes;
                if (state.m_FileBytes % (128 * SectorSize) == 0 || state.m_FileBytes == state.m_FileSize)
                {
                    progress(state);
                }
            }
            if (std::fclose(destination.release()) != 0)
            {
                throw std::runtime_error("Failed to finish output file");
            }
        }
        catch (...)
        {
            std::error_code error;
            std::filesystem::remove(target, error);
            throw;
        }
    }
}
}
