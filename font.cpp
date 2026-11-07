#include "font.hpp"
#include "roboto_data.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <stdexcept>
#include <vector>

Font::Font(SDL_Renderer* renderer) : m_Renderer(renderer)
{
    constexpr int32_t Width = 1024;
    constexpr int32_t Height = 512;
    std::vector<uint8_t> alpha(Width * Height);
    std::array<stbtt_bakedchar, 95> glyphs{};
    if (stbtt_BakeFontBitmap(RobotoFont, 0, 20.0f, alpha.data(), Width, Height, 32, glyphs.size(), glyphs.data()) <= 0)
    {
        throw std::runtime_error("Failed to prepare font");
    }
    std::vector<uint32_t> pixels(alpha.size());
    for (size_t index = 0; index < alpha.size(); index++)
    {
        pixels[index] = 0x00FFFFFF | (static_cast<uint32_t>(alpha[index]) << 24);
    }
    m_Texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, Width, Height);
    if (!m_Texture || !SDL_UpdateTexture(m_Texture, nullptr, pixels.data(), Width * sizeof(uint32_t)) ||
        !SDL_SetTextureBlendMode(m_Texture, SDL_BLENDMODE_BLEND))
    {
        SDL_DestroyTexture(m_Texture);
        m_Texture = nullptr;
        throw std::runtime_error("Failed to create font texture");
    }
    SDL_SetTextureScaleMode(m_Texture, SDL_SCALEMODE_LINEAR);
    for (size_t index = 0; index < glyphs.size(); index++)
    {
        const auto& source = glyphs[index];
        Glyph& target = m_Glyphs[index];
        target.m_Source = {static_cast<float>(source.x0), static_cast<float>(source.y0),
                           static_cast<float>(source.x1 - source.x0), static_cast<float>(source.y1 - source.y0)};
        target.m_XOffset = source.xoff;
        target.m_YOffset = source.yoff;
        target.m_Advance = source.xadvance;
    }
}

Font::~Font()
{
    SDL_DestroyTexture(m_Texture);
}

void Font::Draw(float x, float y, const std::string& text, float scale) const
{
    float position = x;
    for (unsigned char character : text)
    {
        if (character < 32 || character > 126)
        {
            character = '?';
        }
        const Glyph& glyph = m_Glyphs[character - 32];
        const SDL_FRect destination = {position + glyph.m_XOffset * scale, y + (17 + glyph.m_YOffset) * scale,
                                       glyph.m_Source.w * scale, glyph.m_Source.h * scale};
        SDL_RenderTexture(m_Renderer, m_Texture, &glyph.m_Source, &destination);
        position += glyph.m_Advance * scale;
    }
}

float Font::Measure(const std::string& text, float scale) const
{
    float width = 0;
    for (unsigned char character : text)
    {
        if (character < 32 || character > 126)
        {
            character = '?';
        }
        width += m_Glyphs[character - 32].m_Advance * scale;
    }
    return width;
}

void Font::SetColor(uint8_t red, uint8_t green, uint8_t blue) const
{
    SDL_SetTextureColorMod(m_Texture, red, green, blue);
}
