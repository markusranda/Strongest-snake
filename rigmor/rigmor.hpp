#pragma once
#include <iostream>
#include <string>
#include <map>
#include <fstream>
#include <filesystem>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "../libs/stb_image.h"

// ============================================================================
// RIGMOR: Header-only sprite atlas scanner
// Author: Randa
// ============================================================================

// --- Datastructures --------------------------------------------------------

struct AtlasRegion
{
    uint32_t id;
    char name[32];
    uint16_t x;
    uint16_t y;
    uint8_t width;
    uint8_t height;
    char padding[2];
};
static_assert(sizeof(AtlasRegion) == 44);

using CellKey = uint32_t;
CellKey createCellKey(uint16_t cellX, uint16_t cellY)
{
    return (cellX << 16) | (cellY & 0xFFFF);
}
uint16_t getX(CellKey cellKey)
{
    return cellKey >> 16;
}
uint16_t getY(CellKey cellKey)
{
    return cellKey & 0xFFFF;
}

namespace LaunchArg
{
    using namespace std::literals;

    constexpr std::string_view Parse = "scan"sv;
    constexpr std::string_view List = "list"sv;
    constexpr std::string_view Find = "find"sv;
    constexpr std::string_view Edit = "edit"sv;
    constexpr std::string_view Delete = "delete"sv;
}

// --- Helper functions --------------------------------------------------------

void findAtlasRegions(stbi_uc *pixels, int &width, int &height, int &channels, std::map<CellKey, AtlasRegion> &regions)
{
    char atlasRegionName[32] = "<placeholder>";
    uint8_t cellSize = 32;
    int byteCount = width * height * channels; // One pixel is channels number of bytes
    for (uint32_t byte = 0; byte < byteCount; byte += 4)
    {
        uint32_t val = (*(pixels + byte + 0) +
                        *(pixels + byte + 1) +
                        *(pixels + byte + 2) +
                        *(pixels + byte + 3));

        if (val > 0)
        {
            uint32_t pixel = byte / 4;
            uint16_t x = pixel % width; // column
            uint16_t y = pixel / width; // row
            uint32_t cellX = x / 32;
            uint32_t cellY = y / 32;
            CellKey key = createCellKey(cellX, cellY);
            if (regions.find(key) == regions.end())
            {
                AtlasRegion region{};
                region.id = key;
                std::strncpy(region.name, atlasRegionName, sizeof(region.name));
                region.x = cellX;
                region.y = cellY;
                region.width = cellSize;
                region.height = cellSize;
                regions.emplace(key, region);
            }
        }
    }
}

void updateDatabase(std::map<CellKey, AtlasRegion> &regions, std::vector<AtlasRegion> &updatedRegions, const std::string_view pngPath)
{
    // All binary writes use little-endian byte order.
    // | Offset | Field     | Type       | Bytes | Description                           |
    // | -----: | --------- | ---------- | ----- | ------------------------------------- |
    // |      0 | `id`      | `uint32`   | 4     | Unique identifier (never reused)      |
    // |      4 | `name`    | `char[32]` | 32    | Null-terminated or space-padded ASCII |
    // |     36 | `x`       | `uint16`   | 2     | Top-left X position (pixels)          |
    // |     38 | `y`       | `uint16`   | 2     | Top-left Y position (pixels)          |
    // |     40 | `width`   | `uint8`    | 1     | Width in pixels (≤255)                |
    // |     41 | `height`  | `uint8`    | 1     | Height in pixels (≤255)               |
    // |  42–43 | `padding` | `uint8[2]` | 2     | Reserved for flags or alignment       |

    std::filesystem::path p = pngPath;
    std::filesystem::path filename = p.parent_path() / (p.stem().string() + ".rigdb");
    const size_t ROW_LENGTH = 44;

    // TOUCH THE FILE
    if (!std::filesystem::exists(filename))
        std::ofstream(filename, std::ios::binary).close();

    // READ EVERYTHING TO A BUFFER
    std::ifstream in(filename, std::ios::binary | std::ios::ate);
    if (!in)
    {
        throw std::runtime_error("Failed to open file");
    }
    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    if (!in.read(buffer.data(), size))
    {
        throw std::runtime_error("Failed to read file");
    }

    // DO MODIFICATIONS
    std::ofstream out(filename, std::ios::binary | std::ios::app);
    if (!out)
    {
        throw std::runtime_error("Failed to write file");
    }

    for (auto &[keyA, region] : regions)
    {
        // Find out if this region already exists in the buffer
        // It should be possible to match key and the first 4 bytes which is a 32bit key
        // One row is 44 bytes
        bool exists = false;
        for (size_t i = 0; i < buffer.size(); i += ROW_LENGTH)
        {
            uint32_t keyB;
            std::memcpy(&keyB, buffer.data() + i, sizeof(uint32_t));
            if (keyA == keyB)
            {
                exists = true;
                break;
            }
        }

        if (!exists)
        {
            // If it doesn't exist, we can safely append to the end of buffer
            const char *regionBytes = reinterpret_cast<const char *>(&region);
            out.write(regionBytes, sizeof(AtlasRegion));
            updatedRegions.push_back(region);
        }
    }
}

// --- Commands --------------------------------------------------------

void commandScan(const std::string_view pngPath)
{
    std::cout << "Starting scan...\n";
    int width;
    int height;
    int channels;

    stbi_uc *pixels = stbi_load(pngPath.data(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels)
        throw std::runtime_error("failed to load texture image!");

    std::map<CellKey, AtlasRegion> regions;
    std::vector<AtlasRegion> updatedRegions;
    findAtlasRegions(pixels, width, height, channels, regions);
    std::cout << "Found " << regions.size() << " number of sprites\n";
    updateDatabase(regions, updatedRegions, pngPath);
    std::cout << "Updated " << updatedRegions.size() << " number of sprites\n";

    for (AtlasRegion region : updatedRegions)
    {
        std::cout << "New: " << region.x << ", " << region.y << "\n";
    }
}

int main(int argc, char *argv[])
{
    // TODO Deal with these hardcoded values laters
    std::string pngPath = "assets/atlas.png";
    std::string dbPath = "assets/atlas.rigdb";

    if (argc < 2)
    {
        std::cerr << "Usage: rigmor <command>\n";
        return 1;
    }

    try
    {
        char *firstArg = argv[1];
        if (LaunchArg::Parse == firstArg)
        {
            commandScan(pngPath);
        }
        else if (LaunchArg::List == firstArg)
        {
            std::cout << "list is not yet implemented\n";
        }
        else if (LaunchArg::Find == firstArg)
        {
            std::cout << "find is not yet implemented\n";
        }
        else if (LaunchArg::Edit == firstArg)
        {
            std::cout << "edit is not yet implemented\n";
        }
        else if (LaunchArg::Delete == firstArg)
        {
            std::cout << "delete is not yet implemented\n";
        }
        else
        {
            std::cerr << "Unknown command: " << firstArg << "\n";
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}