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

// Rigdb schema
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

// --------------------------------------------------------
// STATIC HELPERS
// --------------------------------------------------------

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#define NOCRYPT
#define NOSERVICE
#define NOMCX
#define NOIME
#define NOSOUND
#define NOKANJI
#define NOHELP
#define NOCOMM
#define NORASTEROPS
#define NOMETAFILE
#define NOWH
#define NOWINMESSAGES
#define NOWINSTYLES
#define NOCLIPBOARD
#define NOCOLOR
#define NODRAWTEXT
#define NOGDICAPMASKS
#define NOICONS
#define NOUSER
#define NONLS
#define NOMB
#define NOOPENFILE
#define NOSCROLL
#define NOSHOWWINDOW
#define NOSYSCOMMANDS
#define NOVIRTUALKEYCODES
#define NOWINOFFSETS
#define NOWINRES
#define NOSYSMETRICS
#define NOTEXTMETRIC
#define NOWINABLE
#include <windows.h>

static std::uint64_t getAvailableMemory()
{
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    GlobalMemoryStatusEx(&status);
    return status.ullAvailPhys; // bytes
}
#endif

#ifdef __linux__
#include <sys/sysinfo.h>

static std::uint64_t getAvailableMemory()
{
    struct sysinfo info{};
    sysinfo(&info);
    return static_cast<std::uint64_t>(info.freeram) * info.mem_unit;
}
#endif

// --------------------------------------------------------
// Datastructures
// --------------------------------------------------------

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
constexpr size_t AtlasRegionSize = sizeof(AtlasRegion);
static_assert(AtlasRegionSize == 44);
static_assert(std::is_trivially_copyable_v<AtlasRegion>);

using CellKey = uint32_t;
CellKey createCellKey(uint16_t cellX, uint16_t cellY, uint16_t numCols)
{
    return cellY * numCols + cellX;
    ;
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

const char DEFAULT_ATLAS_NAME[32] = "<placeholder>";

// --------------------------------------------------------
// Helper functions
// --------------------------------------------------------

void findAtlasRegions(stbi_uc *pixels, int &width, int &height, int &channels, std::map<CellKey, AtlasRegion> &regions)
{
    char atlasRegionName[32]{};
    std::strncpy(atlasRegionName, DEFAULT_ATLAS_NAME, sizeof(atlasRegionName) - 1);
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
            uint32_t cellX = x / cellSize;
            uint32_t cellY = y / cellSize;
            uint32_t numCols = width / cellSize;
            CellKey key = createCellKey(cellX, cellY, numCols);
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

void fileToBuffer(const std::filesystem::path &filename, std::vector<char> &buffer)
{
    std::ifstream in(filename, std::ios::binary | std::ios::ate);
    if (!in) throw std::runtime_error("Failed to open file");

    std::streamsize size = in.tellg();
    if (size > getAvailableMemory()) throw std::runtime_error("Not enough system memory");
    if (size % sizeof(AtlasRegion) != 0) throw std::runtime_error("corrupt db file: size not multiple of AtlasRegion");

    in.seekg(0, std::ios::beg);
    buffer.resize(size);

    if (!in.read(buffer.data(), size)) throw std::runtime_error("Failed to read file");
}

void radix_sort_blocks(std::vector<char> &buffer)
{
    if (buffer.size() % AtlasRegionSize != 0)
        throw std::runtime_error("Invalid buffer size");

    const size_t n = buffer.size() / AtlasRegionSize;
    std::vector<char> temp(buffer.size());

    std::vector<uint32_t> keys(n);
    for (size_t i = 0; i < n; ++i)
        std::memcpy(&keys[i], &buffer[i * AtlasRegionSize], 4);

    constexpr int BITS_PER_PASS = 8;
    constexpr int PASSES = 32 / BITS_PER_PASS;
    constexpr int RADIX = 1 << BITS_PER_PASS;

    std::vector<size_t> count(RADIX), prefix(RADIX);

    for (int pass = 0; pass < PASSES; ++pass)
    {
        std::fill(count.begin(), count.end(), 0);

        const int shift = pass * BITS_PER_PASS;
        for (size_t i = 0; i < n; ++i)
            ++count[(keys[i] >> shift) & (RADIX - 1)];

        prefix[0] = 0;
        for (int i = 1; i < RADIX; ++i)
            prefix[i] = prefix[i - 1] + count[i - 1];

        for (size_t i = 0; i < n; ++i)
        {
            const uint32_t k = (keys[i] >> shift) & (RADIX - 1);
            const size_t pos = prefix[k]++;
            std::memcpy(&temp[pos * AtlasRegionSize], &buffer[i * AtlasRegionSize], AtlasRegionSize);
        }

        buffer.swap(temp);

        for (size_t i = 0; i < n; ++i)
            std::memcpy(&keys[i], &buffer[i * AtlasRegionSize], 4);
    }
}

void updateDatabase(std::map<CellKey, AtlasRegion> &regions, std::vector<AtlasRegion> &updatedRegions, const std::string_view pngPath)
{
    std::filesystem::path p = pngPath;
    std::filesystem::path filename = p.parent_path() / (p.stem().string() + ".rigdb");
    const size_t ROW_LENGTH = 44;

    // TOUCH THE FILE
    if (!std::filesystem::exists(filename)) std::ofstream(filename, std::ios::binary).close();

    // READ EVERYTHING TO A BUFFER
    std::vector<char> buffer;
    fileToBuffer(filename, buffer);

    // DO MODIFICATIONS
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
            buffer.insert(buffer.end(), regionBytes, regionBytes + ROW_LENGTH);
            updatedRegions.push_back(region);
        }
    }

    radix_sort_blocks(buffer);
    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        throw std::runtime_error("Failed to write file");
    }
    out.write(buffer.data(), buffer.size());
    if (!out)
        throw std::runtime_error("Failed to write entire buffer");
}

uint32_t tryParseUint32(const std::string &s)
{
    try
    {
        size_t idx;
        unsigned long val = std::stoul(s, &idx, 10); // base 10

        if (idx != s.size())
            throw std::invalid_argument("Extra characters found");

        if (val > std::numeric_limits<uint32_t>::max())
            throw std::out_of_range("Value exceeds uint32_t");

        return static_cast<uint32_t>(val);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error parsing '" << s << "': " << e.what() << '\n';
        throw;
    }
}

void printHeader()
{
    std::cout << std::left
              << std::setw(10) << "Id"
              << std::setw(32) << "Name"
              << std::setw(4) << "x"
              << std::setw(4) << "y"
              << std::setw(6) << "Width"
              << std::setw(6) << "Height"
              << "\n";
    std::cout << std::string(60, '-') << "\n";
}

void printRegion(AtlasRegion &region)
{
    std::cout << std::left
              << std::setw(10) << static_cast<uint32_t>(region.id)
              << std::setw(32) << region.name
              << std::setw(4) << region.x
              << std::setw(4) << region.y
              << std::setw(6) << static_cast<uint32_t>(region.width)
              << std::setw(6) << static_cast<uint32_t>(region.height)
              << "\n";
}

// --------------------------------------------------------
// Commands
// --------------------------------------------------------

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

void commandList(const std::filesystem::path dbPath, bool onlyMissing = false)
{
    std::vector<char> byteBuffer;
    fileToBuffer(dbPath, byteBuffer);
    AtlasRegion *regions = reinterpret_cast<AtlasRegion *>(byteBuffer.data());
    size_t regionCount = byteBuffer.size() / AtlasRegionSize;

    printHeader();
    for (size_t i = 0; i < regionCount; ++i)
    {
        AtlasRegion region = regions[i];
        if (onlyMissing)
        {
            if (std::strcmp(region.name, DEFAULT_ATLAS_NAME) == 0)
            {
                printRegion(regions[i]);
            }
        }
        else
        {
            printRegion(regions[i]);
        }
    }
}

void commandFind(const std::filesystem::path dbPath, const std::string idStr)
{
    uint32_t id = tryParseUint32(idStr);
    std::vector<char> byteBuffer;
    fileToBuffer(dbPath, byteBuffer);
    AtlasRegion *regions = reinterpret_cast<AtlasRegion *>(byteBuffer.data());
    size_t regionCount = byteBuffer.size() / AtlasRegionSize;

    for (size_t i = 0; i < regionCount; ++i)
    {
        AtlasRegion region = regions[i];
        if (region.id == id)
        {
            printRegion(regions[i]);
            break;
        }
    }
}

void commandEdit(const std::filesystem::path &dbPath, const std::string &idStr, const std::string &name)
{
    if (name.size() > 31)
    {
        std::cerr << "Name too long (max 31 chars)\n";
        return;
    }

    uint32_t id = tryParseUint32(idStr);
    std::fstream file(dbPath, std::ios::in | std::ios::out | std::ios::binary);
    if (!file)
        throw std::runtime_error("Failed to open db file for edit");

    AtlasRegion region;
    size_t offset = 0;
    bool found = false;

    while (file.read(reinterpret_cast<char *>(&region), AtlasRegionSize))
    {
        if (region.id == id)
        {
            found = true;
            std::strncpy(region.name, name.c_str(), sizeof(region.name));
            region.name[31] = '\0';
            file.seekp(offset, std::ios::beg);
            file.write(reinterpret_cast<const char *>(&region), AtlasRegionSize);
            break;
        }
        offset += AtlasRegionSize;
    }

    if (!found)
        std::cerr << "No record found with id " << id << "\n";
    else
        std::cout << "Record updated successfully.\n";
}

/// @brief Deletes all elements inclusive in id range.
/// @param dbPath database file
/// @param idFrom inclusive delete from this id
/// @param idTo omit if deleting one id
void commandDelete(std::filesystem::path dbPath, const std::string idFromStr, const std::string idToStr = "-1")
{
    int idFrom = tryParseUint32(idFromStr);
    int idTo = tryParseUint32(idToStr);
    if (idFrom < 0) throw std::runtime_error("failed to delete - can't delete with negative id");
    if (idTo < 0) idTo = idFrom;
    if (idTo >= 0 && idTo < idFrom) throw std::runtime_error("failed to delete - idTo can't be smaller than idFrom if provided");
    
    // --- LOAD INTO MEMORY ---
    std::vector<char> byteBuffer;
    fileToBuffer(dbPath, byteBuffer);

    const AtlasRegion *regions = reinterpret_cast<AtlasRegion *>(byteBuffer.data());
    const size_t bytesBeforeDelete = byteBuffer.size();
    const size_t regionCount = bytesBeforeDelete / AtlasRegionSize;
    const size_t elementsToDelete = size_t(idTo - idFrom + 1);

    // --- SEARCH AND DESTROY ---
    bool deleted = false;
    for (size_t i = 0; i < regionCount; ++i)
    {
        if (regions[i].id == idFrom) {
            if (i + elementsToDelete > regionCount) throw std::runtime_error("failed to delete - range runs past end of file");
            deleted = true;
            
            const size_t remainingElements = regionCount - (i + elementsToDelete);
            const size_t bytesToMove = remainingElements * AtlasRegionSize;
            
            memmove(
                byteBuffer.data() + i * AtlasRegionSize,
                byteBuffer.data() + (i + elementsToDelete) * AtlasRegionSize,
                bytesToMove
            );
            byteBuffer.resize(byteBuffer.size() - (AtlasRegionSize * elementsToDelete));     
            
            break;
        }
    }

    // --- VALIDATE RESULT ---
    if (!deleted) throw std::runtime_error("failed to delete - failed to find element with provided id");
    size_t byteDiff = bytesBeforeDelete - byteBuffer.size();
    size_t bytesToRemove = AtlasRegionSize * elementsToDelete;
    if(byteDiff != bytesToRemove) throw std::runtime_error("failed to delete - filesize after delete is incorrect");

    // --- WRITE TO FILE ---
    std::ofstream out(dbPath, std::ios::trunc | std::ios::binary);
    if (!out) throw std::runtime_error("failed to delete - couldn't to open output file");
    out.write(byteBuffer.data(), static_cast<std::streamsize>(byteBuffer.size()));
    if (!out) throw std::runtime_error("failed to delete - couldn't to write output file");

    fprintf(stdout, "Delete was successful\n");
}

// --------------------------------------------------------
// Entrypoint
// --------------------------------------------------------

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
        if (LaunchArg::Parse == std::string_view(firstArg))
        {
            commandScan(pngPath);
        }
        else if (LaunchArg::List == std::string_view(firstArg))
        {
            if (argc == 3)
            {
                if (std::strcmp(argv[2], "--missing") == 0)
                {
                    commandList(dbPath, true);
                    return 0;
                }

                std::cerr << "Usage: rigmor find --missing\n";
                return 1;
            }

            commandList(dbPath);
        }
        else if (LaunchArg::Find == std::string_view(firstArg))
        {
            if (argc < 3)
            {
                std::cerr << "Usage: rigmor find\n";
                return 1;
            }
            commandFind(dbPath, argv[2]);
        }
        else if (LaunchArg::Edit == std::string_view(firstArg))
        {
            if (argc < 4)
            {
                std::cerr << "Usage: rigmor edit <id> <name>\n";
                return 1;
            }
            commandEdit(dbPath, argv[2], argv[3]);
        }
        else if (LaunchArg::Delete == std::string_view(firstArg))
        {
            if (argc < 3 || argc > 4)
            {
                std::cerr << "Usage: rigmor delete <id> | rigmor delete <idFrom> <idTo>\n";
                return 1;
            }
            if (argc == 4) commandDelete(dbPath, argv[2], argv[3]);
            else commandDelete(dbPath, argv[2]);
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