#pragma once

#include "gdi.hpp"

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace DiscExtractor
{
struct File
{
    std::filesystem::path m_Path;
    uint32_t m_Sector = 0;
    uint32_t m_Size = 0;
};

struct Details
{
    std::string m_Title;
    std::string m_Region;
    std::string m_GameId;
    std::string m_ReleaseDate;
    std::string m_Publisher;
};

struct Progress
{
    std::string m_File;
    uint64_t m_FileBytes = 0;
    uint64_t m_FileSize = 0;
    uint64_t m_TotalBytes = 0;
    uint64_t m_TotalSize = 0;
    size_t m_FileIndex = 0;
    size_t m_FileCount = 0;
};

class Image
{
public:
    explicit Image(const std::filesystem::path& path);
    const Details& GetDetails() const;
    const std::vector<File>& GetFiles() const;
    uint64_t GetSize() const;
    size_t GetTrackCount() const;
    void Extract(const std::filesystem::path& output, const std::atomic_bool& cancelled,
                 const std::function<void(const Progress&)>& progress) const;

private:
    Disc m_Disc;
    Track m_DataTrack;
    uint32_t m_BaseSector = 0;
    Details m_Details;
    std::vector<File> m_Files;
};
}
