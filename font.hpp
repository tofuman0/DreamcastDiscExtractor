#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <string>

class Font
{
public:
    explicit Font(SDL_Renderer* renderer);
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    void Draw(float x, float y, const std::string& text, float scale = 1.0f) const;
    float Measure(const std::string& text, float scale = 1.0f) const;
    void SetColor(uint8_t red, uint8_t green, uint8_t blue) const;

private:
    struct Glyph
    {
        SDL_FRect m_Source{};
        float m_XOffset = 0;
        float m_YOffset = 0;
        float m_Advance = 0;
    };
    SDL_Renderer* m_Renderer = nullptr;
    SDL_Texture* m_Texture = nullptr;
    std::array<Glyph, 95> m_Glyphs{};
};
