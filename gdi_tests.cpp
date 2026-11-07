#include "gdi.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void WriteFile(const std::filesystem::path& path, const std::string& data)
{
    std::ofstream output(path, std::ios::binary);
    output << data;
    if (!output)
    {
        throw std::runtime_error("Failed to write test fixture");
    }
}

void RequireFailure(const std::filesystem::path& path)
{
    try
    {
        DiscExtractor::ReadGdi(path);
    }
    catch (const std::runtime_error&)
    {
        return;
    }
    throw std::runtime_error("Invalid GDI was accepted");
}
}

int main(int argc, char**)
{
    if (argc != 1)
    {
        return 1;
    }
    const std::filesystem::path directory = std::filesystem::current_path() / "gdi-test-fixtures";
    std::filesystem::create_directories(directory);
    const auto descriptor = directory / "test.gdi";
    try
    {
        WriteFile(directory / "track 01.bin", std::string(2352, 'a'));
        WriteFile(directory / "track03.bin", std::string(4096, 'b'));
        WriteFile(descriptor, "2\n1 0 0 2352 \"track 01.bin\" 0\n2 45000 4 2048 track03.bin 0\n");
        const DiscExtractor::Disc disc = DiscExtractor::ReadGdi(descriptor);
        if (disc.m_Tracks.size() != 2 || disc.m_Size != 6448 || disc.m_Tracks[1].m_StartSector != 45000)
        {
            throw std::runtime_error("Valid GDI was read incorrectly");
        }
        WriteFile(descriptor, "1\n1 0 4 2048 missing.bin 0\n");
        RequireFailure(descriptor);
        WriteFile(descriptor, "1\n1 0 4 2048 track03.bin 4096\n");
        RequireFailure(descriptor);
        WriteFile(descriptor, "2\n1 0 4 2048 track03.bin 0\n");
        RequireFailure(descriptor);
        WriteFile(descriptor, "1\n1 0 4 2048 track03.bin 0\n2 1 4 2048 track03.bin 0\n");
        RequireFailure(descriptor);
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
