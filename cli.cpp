#include "cli.hpp"
#include "image.hpp"

#include <atomic>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#endif

namespace
{
void PrepareConsole()
{
#ifdef _WIN32
    const HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    const bool hasOutput = output != nullptr && output != INVALID_HANDLE_VALUE && GetFileType(output) != FILE_TYPE_UNKNOWN;
    if (!hasOutput && AttachConsole(ATTACH_PARENT_PROCESS))
    {
        std::FILE* stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
    }
#endif
}

void PrintUsage()
{
    std::cout << "Dreamcast Disc Extractor\n\n"
              << "Usage:\n"
              << "  Dreamcast Disc Extractor --extract <image> --output <folder>\n"
              << "  Dreamcast Disc Extractor --info <image>\n"
              << "  Dreamcast Disc Extractor --help\n";
}

std::filesystem::path DecodePath(const char* value)
{
    const std::string text(value);
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

void PrintDetails(const DiscExtractor::Image& image)
{
    const auto& details = image.GetDetails();
    const auto value = [](const std::string& text)
    {
        return text.empty() ? std::string("Unknown") : text;
    };
    std::cout << "Title: " << value(details.m_Title) << '\n'
              << "Region: " << value(details.m_Region) << '\n'
              << "Game ID: " << value(details.m_GameId) << '\n'
              << "Release date: " << value(details.m_ReleaseDate) << '\n'
              << "Publisher: " << value(details.m_Publisher) << '\n'
              << "Tracks: " << image.GetTrackCount() << '\n'
              << "Files: " << image.GetFiles().size() << '\n';
}
}

int32_t RunCommandLine(int32_t argumentCount, char** arguments)
{
    PrepareConsole();
    if (argumentCount == 2 && std::string_view(arguments[1]) == "--help")
    {
        PrintUsage();
        return 0;
    }

    std::filesystem::path input;
    std::filesystem::path output;
    bool infoOnly = false;
    for (int32_t index = 1; index < argumentCount; index++)
    {
        const std::string_view option(arguments[index]);
        if ((option == "--extract" || option == "--info") && index + 1 < argumentCount && input.empty())
        {
            input = DecodePath(arguments[++index]);
            infoOnly = option == "--info";
        }
        else if (option == "--output" && index + 1 < argumentCount && output.empty())
        {
            output = DecodePath(arguments[++index]);
        }
        else
        {
            std::cerr << "Invalid option: " << option << "\n\n";
            PrintUsage();
            return 2;
        }
    }
    if (input.empty() || (infoOnly && !output.empty()) || (!infoOnly && output.empty()))
    {
        std::cerr << "Choose an image and output folder.\n\n";
        PrintUsage();
        return 2;
    }

    try
    {
        const DiscExtractor::Image image(input);
        PrintDetails(image);
        if (!infoOnly)
        {
            const std::atomic_bool cancelled = false;
            image.Extract(output, cancelled, [](const DiscExtractor::Progress& progress)
            {
                if (progress.m_FileBytes == 0)
                {
                    std::cout << '[' << progress.m_FileIndex << '/' << progress.m_FileCount << "] " << progress.m_File << '\n';
                }
            });
            std::cout << "Extraction complete.\n";
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << (infoOnly ? "Failed to inspect image: " : "Extraction failed: ") << error.what() << '\n';
        return 1;
    }
}
