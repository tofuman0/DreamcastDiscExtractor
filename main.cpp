#include "image.hpp"
#include "font.hpp"
#include "icon.hpp"
#include "cli.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>
#include <mutex>
#include <string>
#include <thread>

namespace
{
struct App
{
    SDL_Window* m_Window = nullptr;
    SDL_Renderer* m_Renderer = nullptr;
    std::unique_ptr<Font> m_Font;
    std::mutex m_Mutex;
    std::string m_Input;
    std::string m_Output;
    int32_t m_EditField = 0;
    float m_Width = 960;
    float m_Height = 680;
    std::string m_Message = "Choose a disc image to begin.";
    std::unique_ptr<DiscExtractor::Image> m_Image;
    DiscExtractor::Progress m_Progress;
    std::atomic_bool m_Cancelled = false;
    std::atomic_bool m_Working = false;
    std::atomic_bool m_DialogOpen = false;
    std::chrono::steady_clock::time_point m_Started;
    double m_Elapsed = 0;
    std::thread m_Worker;
};

void DrawRect(SDL_Renderer* renderer, float x, float y, float width, float height, uint8_t red, uint8_t green, uint8_t blue)
{
    SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
    const SDL_FRect rect{x, y, width, height};
    SDL_RenderFillRect(renderer, &rect);
}

void DrawText(App& app, float x, float y, const std::string& text, float scale = 1.0f,
              uint8_t red = 236, uint8_t green = 242, uint8_t blue = 251)
{
    app.m_Font->SetColor(red, green, blue);
    app.m_Font->Draw(x, y, text, scale);
}

void DrawPanel(App& app, float x, float y, float width, float height, const std::string& title)
{
    DrawRect(app.m_Renderer, x, y, width, height, 36, 53, 77);
    DrawRect(app.m_Renderer, x + 1, y + 1, width - 2, height - 2, 13, 23, 38);
    DrawRect(app.m_Renderer, x + 1, y + 1, width - 2, 32, 24, 40, 63);
    DrawText(app, x + 15, y + 7, title, 0.9f);
}

void DrawButton(App& app, float x, float y, float width, float height, const std::string& label,
                bool enabled, bool primary = false)
{
    if (primary && enabled)
    {
        DrawRect(app.m_Renderer, x, y, width, height, 40, 140, 226);
        DrawRect(app.m_Renderer, x + 1, y + 1, width - 2, height - 2, 18, 91, 168);
    }
    else
    {
        DrawRect(app.m_Renderer, x, y, width, height, 57, 75, 100);
        DrawRect(app.m_Renderer, x + 1, y + 1, width - 2, height - 2,
                 enabled ? 33 : 21, enabled ? 48 : 30, enabled ? 70 : 45);
    }
    const float textWidth = app.m_Font->Measure(label, 0.9f);
    DrawText(app, x + (width - textWidth) * 0.5f, y + (height - 18) * 0.5f,
             label, 0.9f, enabled ? 239 : 127, enabled ? 245 : 143, enabled ? 253 : 165);
}

std::string ShortenPath(const Font& font, const std::string& text, float maxWidth)
{
    if (font.Measure(text) <= maxWidth)
    {
        return text;
    }
    size_t start = 0;
    while (start < text.size())
    {
        start++;
        while (start < text.size() && (static_cast<uint8_t>(text[start]) & 0xC0) == 0x80)
        {
            start++;
        }
        const std::string shortened = "..." + text.substr(start);
        if (font.Measure(shortened) <= maxWidth)
        {
            return shortened;
        }
    }
    return "...";
}

void Render(App& app)
{
    std::lock_guard lock(app.m_Mutex);
    const float width = app.m_Width;
    const float height = app.m_Height;
    const float leftWidth = (width - 60) * 0.42f;
    const float rightX = 36 + leftWidth;
    const float rightWidth = width - rightX - 24;
    const float buttonY = height - 63;
    const float activityY = 526;
    const float activityHeight = std::max(65.0f, buttonY - activityY - 12);

    SDL_SetRenderDrawColor(app.m_Renderer, 7, 14, 25, 255);
    SDL_RenderClear(app.m_Renderer);
    DrawRect(app.m_Renderer, 0, 0, width, 78, 18, 29, 46);
    DrawRect(app.m_Renderer, 0, 76, width, 2, 235, 117, 31);
    DrawRect(app.m_Renderer, 24, 18, 7, 37, 242, 126, 32);
    DrawRect(app.m_Renderer, 35, 18, 7, 37, 242, 126, 32);
    DrawText(app, 55, 15, "Dreamcast Disc Extractor", 1.35f);
    DrawText(app, 56, 47, "Extract your game image to a data folder", 0.8f, 165, 181, 203);

    DrawPanel(app, 24, 90, width - 48, 76, "ROM IN  /  GDI  -  CUE  -  BIN  -  CHD  -  CDI");
    DrawRect(app.m_Renderer, 38, 132, width - 220, 26, 7, 15, 27);
    const float pathLimit = width - 245;
    const std::string input = app.m_Input.empty() ? "Choose an image" : ShortenPath(*app.m_Font, app.m_Input, pathLimit);
    const float inputX = app.m_Input.empty() ? 46 : width - 190 - app.m_Font->Measure(input, 0.9f);
    DrawText(app, std::max(46.0f, inputX), 136, input, 0.9f);
    DrawButton(app, width - 166, 128, 128, 32, "Browse", !app.m_Working);

    DrawPanel(app, 24, 176, width - 48, 76, "DATA OUT");
    DrawRect(app.m_Renderer, 38, 218, width - 220, 26, 7, 15, 27);
    const std::string output = app.m_Output.empty() ? "Choose a folder" : ShortenPath(*app.m_Font, app.m_Output, pathLimit);
    const float outputX = app.m_Output.empty() ? 46 : width - 190 - app.m_Font->Measure(output, 0.9f);
    DrawText(app, std::max(46.0f, outputX), 222, output, 0.9f);
    DrawButton(app, width - 166, 214, 128, 32, "Browse", !app.m_Working);

    DrawPanel(app, 24, 264, leftWidth, 250, "ROM DETAILS");
    if (app.m_Image)
    {
        const auto& details = app.m_Image->GetDetails();
        const auto show = [](const std::string& value) { return value.empty() ? std::string("Unknown") : value; };
        const float valueX = 150;
        DrawText(app, 40, 307, "Title", 0.85f, 156, 176, 198);
        DrawText(app, valueX, 307, ShortenPath(*app.m_Font, show(details.m_Title), leftWidth - 140), 0.85f);
        DrawText(app, 40, 334, "Region", 0.85f, 156, 176, 198);
        DrawText(app, valueX, 334, show(details.m_Region), 0.85f);
        DrawText(app, 40, 361, "Game ID", 0.85f, 156, 176, 198);
        DrawText(app, valueX, 361, show(details.m_GameId), 0.85f);
        DrawText(app, 40, 388, "Date", 0.85f, 156, 176, 198);
        DrawText(app, valueX, 388, show(details.m_ReleaseDate), 0.85f);
        DrawText(app, 40, 415, "Publisher", 0.85f, 156, 176, 198);
        DrawText(app, valueX, 415, ShortenPath(*app.m_Font, show(details.m_Publisher), leftWidth - 140), 0.85f);
        DrawText(app, 40, 442, "Tracks", 0.85f, 156, 176, 198);
        DrawText(app, valueX, 442, std::to_string(app.m_Image->GetTrackCount()) + "    Files: " +
                 std::to_string(app.m_Image->GetFiles().size()), 0.85f);
    }
    else
    {
        DrawText(app, 40, 310, "Select an image to see its details.", 0.85f, 156, 176, 198);
    }

    DrawPanel(app, rightX, 264, rightWidth, 250, "EXTRACTION");
    const float barX = rightX + 16;
    const float barWidth = rightWidth - 32;
    const float filePart = app.m_Progress.m_FileSize ? static_cast<float>(app.m_Progress.m_FileBytes) / app.m_Progress.m_FileSize : 0;
    const float totalPart = app.m_Progress.m_TotalSize ? static_cast<float>(app.m_Progress.m_TotalBytes) / app.m_Progress.m_TotalSize : 0;
    const std::string current = app.m_Progress.m_File.empty() ? "Waiting to extract" : app.m_Progress.m_File;
    DrawText(app, barX, 307, ShortenPath(*app.m_Font, current, barWidth - 20), 0.85f, 179, 194, 213);
    DrawText(app, barX, 339, "Current file", 0.8f, 156, 176, 198);
    DrawRect(app.m_Renderer, barX, 364, barWidth, 12, 36, 50, 70);
    DrawRect(app.m_Renderer, barX, 364, barWidth * filePart, 12, 28, 147, 240);
    DrawText(app, barX, 385, "Total progress", 0.8f, 156, 176, 198);
    DrawRect(app.m_Renderer, barX, 410, barWidth, 12, 36, 50, 70);
    DrawRect(app.m_Renderer, barX, 410, barWidth * totalPart, 12, 28, 147, 240);
    DrawText(app, barX, 434, "Files: " + std::to_string(app.m_Progress.m_FileIndex) + " / " +
             std::to_string(app.m_Progress.m_FileCount) + "    " + std::to_string(static_cast<int32_t>(totalPart * 100)) + "%", 0.8f);
    if (app.m_Progress.m_TotalBytes > 0)
    {
        const double elapsed = app.m_Working ?
            std::chrono::duration<double>(std::chrono::steady_clock::now() - app.m_Started).count() : app.m_Elapsed;
        const double speed = elapsed > 0 ? app.m_Progress.m_TotalBytes / elapsed : 0;
        const double remaining = speed > 0 ? (app.m_Progress.m_TotalSize - app.m_Progress.m_TotalBytes) / speed : 0;
        std::ostringstream stats;
        stats << std::fixed << std::setprecision(1) << app.m_Progress.m_TotalBytes / (1024.0 * 1024.0)
              << " / " << app.m_Progress.m_TotalSize / (1024.0 * 1024.0) << " MiB    "
              << speed / (1024 * 1024) << " MiB/s";
        DrawText(app, barX, 461, stats.str(), 0.8f, 156, 176, 198);
        std::ostringstream timing;
        timing << std::fixed << std::setprecision(0) << elapsed << "s elapsed    " << remaining << "s remaining";
        DrawText(app, barX, 484, timing.str(), 0.8f, 156, 176, 198);
    }

    DrawPanel(app, 24, activityY, width - 48, activityHeight, "ACTIVITY");
    DrawText(app, 40, activityY + 40, ShortenPath(*app.m_Font, app.m_Message, width - 80), 0.85f, 179, 194, 213);
    DrawButton(app, width - 322, buttonY, 140, 40, "Extract", app.m_Image && !app.m_Working, true);
    DrawButton(app, width - 168, buttonY, 144, 40, "Abort", app.m_Working);
    SDL_RenderPresent(app.m_Renderer);
}

void SDLCALL SelectInput(void* userdata, const char* const* paths, int32_t)
{
    auto& app = *static_cast<App*>(userdata);
    if (paths == nullptr || paths[0] == nullptr)
    {
        app.m_DialogOpen = false;
        return;
    }
    const std::string path = paths[0];
    try
    {
        auto image = std::make_unique<DiscExtractor::Image>(std::filesystem::path(std::u8string(path.begin(), path.end())));
        std::lock_guard lock(app.m_Mutex);
        app.m_Input = path;
        app.m_Image = std::move(image);
        app.m_Message = "Image inspected. Choose an output folder.";
    }
    catch (const std::exception& error)
    {
        std::lock_guard lock(app.m_Mutex);
        app.m_Input = path;
        app.m_Image.reset();
        app.m_Message = error.what();
    }
    app.m_DialogOpen = false;
}

void SDLCALL SelectOutput(void* userdata, const char* const* paths, int32_t)
{
    auto& app = *static_cast<App*>(userdata);
    if (paths != nullptr && paths[0] != nullptr)
    {
        std::lock_guard lock(app.m_Mutex);
        app.m_Output = paths[0];
    }
    app.m_DialogOpen = false;
}

void Start(App& app)
{
    std::lock_guard lock(app.m_Mutex);
    if (app.m_Working || app.m_DialogOpen)
    {
        return;
    }
    if (!app.m_Image)
    {
        app.m_Message = "Choose a disc image first.";
        return;
    }
    if (app.m_Output.empty())
    {
        app.m_Message = "Choose an output folder first.";
        return;
    }
    if (app.m_Worker.joinable())
    {
        app.m_Worker.join();
    }
    app.m_Cancelled = false;
    app.m_Working = true;
    app.m_Progress = {};
    app.m_Started = std::chrono::steady_clock::now();
    app.m_Elapsed = 0;
    app.m_Message = "Extraction started.";
    app.m_Worker = std::thread([&app]
    {
        try
        {
            const auto output = std::filesystem::path(std::u8string(app.m_Output.begin(), app.m_Output.end()));
            app.m_Image->Extract(output, app.m_Cancelled, [&app](const DiscExtractor::Progress& progress)
            {
                std::lock_guard lock(app.m_Mutex);
                app.m_Progress = progress;
            });
            std::lock_guard lock(app.m_Mutex);
            app.m_Elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - app.m_Started).count();
            app.m_Message = "Extraction complete.";
        }
        catch (const std::exception& error)
        {
            std::lock_guard lock(app.m_Mutex);
            app.m_Elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - app.m_Started).count();
            app.m_Message = error.what();
        }
        app.m_Working = false;
    });
}
}

int main(int argc, char** argv)
{
    if (argc > 1)
    {
        return RunCommandLine(argc, argv);
    }
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        return 1;
    }
    App app;
    app.m_Window = SDL_CreateWindow("Dreamcast Disc Extractor", 960, 680, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!app.m_Window)
    {
        SDL_Quit();
        return 1;
    }
    SetApplicationIcon(app.m_Window);
    app.m_Renderer = SDL_CreateRenderer(app.m_Window, nullptr);
    if (!app.m_Renderer)
    {
        SDL_DestroyWindow(app.m_Window);
        SDL_Quit();
        return 1;
    }
    try
    {
        app.m_Font = std::make_unique<Font>(app.m_Renderer);
    }
    catch (const std::exception& error)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Dreamcast Disc Extractor", error.what(), app.m_Window);
        SDL_DestroyRenderer(app.m_Renderer);
        SDL_DestroyWindow(app.m_Window);
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(app.m_Window, 800, 600);
    bool running = true;
    while (running)
    {
        int32_t windowWidth = 0;
        int32_t windowHeight = 0;
        SDL_GetWindowSize(app.m_Window, &windowWidth, &windowHeight);
        if (windowWidth <= 0 || windowHeight <= 0)
        {
            windowWidth = 960;
            windowHeight = 680;
        }
        const float scale = std::min(static_cast<float>(windowWidth) / 960, static_cast<float>(windowHeight) / 680);
        const int32_t logicalWidth = static_cast<int32_t>(windowWidth / scale);
        const int32_t logicalHeight = static_cast<int32_t>(windowHeight / scale);
        app.m_Width = static_cast<float>(logicalWidth);
        app.m_Height = static_cast<float>(logicalHeight);
        SDL_SetRenderLogicalPresentation(app.m_Renderer, logicalWidth, logicalHeight, SDL_LOGICAL_PRESENTATION_STRETCH);
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            SDL_ConvertEventToRenderCoordinates(app.m_Renderer, &event);
            if (event.type == SDL_EVENT_QUIT)
            {
                if (!app.m_DialogOpen)
                {
                    running = false;
                }
            }
            else if (event.type == SDL_EVENT_TEXT_INPUT && app.m_EditField != 0 && !app.m_Working && !app.m_DialogOpen)
            {
                std::lock_guard lock(app.m_Mutex);
                std::string& text = app.m_EditField == 1 ? app.m_Input : app.m_Output;
                text += event.text.text;
                if (app.m_EditField == 1)
                {
                    app.m_Image.reset();
                    app.m_Message = "Press Enter to inspect the image.";
                }
            }
            else if (event.type == SDL_EVENT_KEY_DOWN && app.m_EditField != 0 && !app.m_Working && !app.m_DialogOpen)
            {
                if (event.key.key == SDLK_BACKSPACE)
                {
                    std::lock_guard lock(app.m_Mutex);
                    std::string& text = app.m_EditField == 1 ? app.m_Input : app.m_Output;
                    if (!text.empty())
                    {
                        size_t position = text.size() - 1;
                        while (position > 0 && (static_cast<uint8_t>(text[position]) & 0xC0) == 0x80)
                        {
                            position--;
                        }
                        text.erase(position);
                        if (app.m_EditField == 1)
                        {
                            app.m_Image.reset();
                            app.m_Message = "Press Enter to inspect the image.";
                        }
                    }
                }
                else if (event.key.key == SDLK_RETURN)
                {
                    if (app.m_EditField == 1)
                    {
                        const char* paths[] = {app.m_Input.c_str(), nullptr};
                        app.m_DialogOpen = true;
                        SelectInput(&app, paths, 0);
                    }
                    app.m_EditField = 0;
                    SDL_StopTextInput(app.m_Window);
                }
            }
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT)
            {
                const float x = event.button.x;
                const float y = event.button.y;
                if (!app.m_Working && !app.m_DialogOpen && x >= 38 && x < app.m_Width - 182 && y >= 128 && y <= 160)
                {
                    app.m_EditField = 1;
                    SDL_StartTextInput(app.m_Window);
                }
                else if (!app.m_Working && !app.m_DialogOpen && x >= 38 && x < app.m_Width - 182 && y >= 214 && y <= 246)
                {
                    app.m_EditField = 2;
                    SDL_StartTextInput(app.m_Window);
                }
                else if (!app.m_Working && !app.m_DialogOpen && x >= app.m_Width - 166 && x <= app.m_Width - 38 && y >= 128 && y <= 160)
                {
                    app.m_EditField = 0;
                    SDL_StopTextInput(app.m_Window);
                    app.m_DialogOpen = true;
                    static const SDL_DialogFileFilter Filters[] = {{"Disc images", "gdi;cue;bin;chd;cdi"}};
                    SDL_ShowOpenFileDialog(SelectInput, &app, app.m_Window, Filters, 1, nullptr, false);
                }
                else if (!app.m_Working && !app.m_DialogOpen && x >= app.m_Width - 166 && x <= app.m_Width - 38 && y >= 214 && y <= 246)
                {
                    app.m_EditField = 0;
                    SDL_StopTextInput(app.m_Window);
                    app.m_DialogOpen = true;
                    SDL_ShowOpenFolderDialog(SelectOutput, &app, app.m_Window, nullptr, false);
                }
                else if (x >= app.m_Width - 322 && x <= app.m_Width - 182 && y >= app.m_Height - 63 && y <= app.m_Height - 23)
                {
                    Start(app);
                }
                else if (x >= app.m_Width - 168 && x <= app.m_Width - 24 && y >= app.m_Height - 63 && y <= app.m_Height - 23)
                {
                    app.m_Cancelled = true;
                }
            }
        }
        Render(app);
        SDL_Delay(16);
    }
    app.m_Cancelled = true;
    if (app.m_Worker.joinable())
    {
        app.m_Worker.join();
    }
    app.m_Font.reset();
    SDL_DestroyRenderer(app.m_Renderer);
    SDL_DestroyWindow(app.m_Window);
    SDL_Quit();
    return 0;
}
