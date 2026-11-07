#pragma once

#include "gdi.hpp"

namespace DiscExtractor
{
Disc ReadCue(const std::filesystem::path& path);
Disc ReadBin(const std::filesystem::path& path);
}
