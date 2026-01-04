#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <iostream>

// --------------------------------------------------------
// DATASTRUCTURES
// --------------------------------------------------------

// Assumes little-endian Windows.
#pragma pack(push, 1)
struct AtlasRegionDisk
{
    std::uint32_t id;
    char name[32];
    std::uint16_t x;
    std::uint16_t y;
    std::uint8_t width;
    std::uint8_t height;
    char padding[2];
};
#pragma pack(pop)
static_assert(sizeof(AtlasRegionDisk) == 44);

// --------------------------------------------------------
// STATE
// --------------------------------------------------------

static std::filesystem::path rigmorExe;
static std::filesystem::path testDir;
static std::filesystem::path db;
static std::filesystem::path png;
static std::filesystem::path cfg;

// --------------------------------------------------------
// HELPERS
// --------------------------------------------------------

static AtlasRegionDisk mk(std::uint32_t id, const char* name)
{
    AtlasRegionDisk r{};
    r.id = id;
    std::memset(r.name, 0, sizeof(r.name));
    std::strncpy(r.name, name, sizeof(r.name) - 1);
    r.x = 0;
    r.y = 0;
    r.width = 32;
    r.height = 32;
    r.padding[0] = 0;
    r.padding[1] = 0;
    return r;
}

static std::string read_all_bytes(const std::filesystem::path& p)
{
    std::ifstream in(p, std::ios::binary);
    if (!in) throw std::runtime_error("Failed to read file " + p.string());

    in.seekg(0, std::ios::end);
    const std::streamsize sz = in.tellg();
    if (sz < 0) throw std::runtime_error("tellg failed for " + p.string());
    in.seekg(0, std::ios::beg);

    std::string bytes;
    bytes.resize(static_cast<std::size_t>(sz));
    if (sz > 0) in.read(bytes.data(), sz);

    if (!in && sz > 0) throw std::runtime_error("Failed to read entire file " + p.string());
    return bytes;
}

static std::size_t rigdb_count(const std::filesystem::path& p)
{
    const auto sz = std::filesystem::file_size(p);
    if (sz % sizeof(AtlasRegionDisk) != 0)
        throw std::runtime_error("rigdb corrupted: size not multiple of 44");
    return static_cast<std::size_t>(sz / sizeof(AtlasRegionDisk));
}

static AtlasRegionDisk rigdb_at(const std::filesystem::path& p, std::size_t index)
{
    const std::string bytes = read_all_bytes(p);
    if (bytes.size() % sizeof(AtlasRegionDisk) != 0)
        throw std::runtime_error("rigdb corrupted: size not multiple of 44");

    const std::size_t count = bytes.size() / sizeof(AtlasRegionDisk);
    if (index >= count) throw std::runtime_error("rigdb_at out of range");

    AtlasRegionDisk r{};
    std::memcpy(&r, bytes.data() + index * sizeof(AtlasRegionDisk), sizeof(AtlasRegionDisk));
    return r;
}

static bool rigdb_has_id(const std::filesystem::path& p, std::uint32_t id)
{
    const std::string bytes = read_all_bytes(p);
    if (bytes.size() % sizeof(AtlasRegionDisk) != 0)
        throw std::runtime_error("rigdb corrupted: size not multiple of 44");

    const std::size_t count = bytes.size() / sizeof(AtlasRegionDisk);
    for (std::size_t i = 0; i < count; ++i)
    {
        AtlasRegionDisk r{};
        std::memcpy(&r, bytes.data() + i * sizeof(AtlasRegionDisk), sizeof(AtlasRegionDisk));
        if (r.id == id) return true;
    }
    return false;
}

static std::string rigdb_name_for_id(const std::filesystem::path& p, std::uint32_t id)
{
    const std::string bytes = read_all_bytes(p);
    if (bytes.size() % sizeof(AtlasRegionDisk) != 0)
        throw std::runtime_error("rigdb corrupted: size not multiple of 44");

    const std::size_t count = bytes.size() / sizeof(AtlasRegionDisk);
    for (std::size_t i = 0; i < count; ++i)
    {
        AtlasRegionDisk r{};
        std::memcpy(&r, bytes.data() + i * sizeof(AtlasRegionDisk), sizeof(AtlasRegionDisk));
        if (r.id == id)
        {
            char tmp[33]{};
            std::memcpy(tmp, r.name, 32);
            tmp[32] = '\0';
            return std::string(tmp);
        }
    }
    throw std::runtime_error("rigdb_name_for_id: id not found");
}

static int run_capture_stdout(
    const std::filesystem::path& exe,
    const std::filesystem::path& workdir,
    const std::string& args,
    std::string& outText,
    bool muted = false
)
{
    // Use redirection to a file to keep this minimal and portable
    const auto outFile = workdir / "stdout.txt";
    const auto errFile = workdir / "stderr.txt";

    // Quote paths for Windows
    auto q = [](const std::filesystem::path& p) {
        std::string s = p.string();
        std::string r;
        r.reserve(s.size() + 2);
        r.push_back('"');
        for (char c : s) r.push_back(c);
        r.push_back('"');
        return r;
    };

    const std::string cmd =
        "cmd /c \"cd /d " + q(workdir) + " && " +
        q(exe) + " " + args +
        " > " + q(outFile) + " 2> " + q(errFile) + "\"";

    const int rc = std::system(cmd.c_str());

    // Read stdout
    {
        std::ifstream in(outFile, std::ios::binary);
        outText.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    // If it failed, also dump stderr to help debugging
    if (rc != 0)
    {
        std::string errText;
        std::ifstream ein(errFile, std::ios::binary);
        errText.assign((std::istreambuf_iterator<char>(ein)), std::istreambuf_iterator<char>());

        if (!muted) {
            fprintf(stderr, "Command failed (rc=%d): %s\n", rc, cmd.c_str());
            if (!errText.empty()) fprintf(stderr, "stderr:\n%s\n", errText.c_str());
        }
    }

    return rc;
}

static void write_text_file(const std::filesystem::path& p, const std::string& s)
{
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Failed to write: " + p.string());
    out.write(s.data(), (std::streamsize)s.size());
}

static bool contains(const std::string& hay, const std::string& needle)
{
    return hay.find(needle) != std::string::npos;
}

static void fail(const char* msg)
{
    throw std::runtime_error("TEST FAIL: " + std::string(msg));
}

static void write_rigdb(const std::filesystem::path& p, const AtlasRegionDisk* regions, std::size_t count)
{
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Failed to write rigdb " + p.string());

    out.write(reinterpret_cast<const char*>(regions),
              static_cast<std::streamsize>(count * sizeof(AtlasRegionDisk)));
    if (!out) throw std::runtime_error("Failed to write entire rigdb " + p.string());
}

static void seed_db_basic()
{
    // 3 records: includes placeholder for list --missing
    const char DEFAULT_ATLAS_NAME[32] = "<placeholder>";
    AtlasRegionDisk regions[3] = {
        mk(10, "alpha"),
        mk(20, DEFAULT_ATLAS_NAME),
        mk(30, "gamma"),
    };
    write_rigdb(db, regions, 3);
}

static void seed_db_delete_case()
{
    AtlasRegionDisk regions[6] = {
        mk(10, "a"),
        mk(11, "b"),
        mk(12, "c"),
        mk(13, "d"),
        mk(14, "e"),
        mk(15, "f"),
    };
    write_rigdb(db, regions, 6);
}

static void seed_db_empty()
{
    std::ofstream out(db, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Failed to create empty db " + db.string());
}

// --------------------------------------------------------
// TESTS
// --------------------------------------------------------

static void testList() {
    seed_db_basic();

    std::string out;
    int rc = run_capture_stdout(rigmorExe, testDir, "list", out);
    if (rc != 0) fail("rigmor list returned non-zero");

    // Basic header check (keep it loose so formatting tweaks don't break tests)
    if (!contains(out, "Id") || !contains(out, "Name") || !contains(out, "Width") || !contains(out, "Height"))
        fail("rigmor list output missing header columns");

    // Expect rows
    if (!contains(out, "10") || !contains(out, "alpha")) fail("rigmor list missing id=10 alpha");
    if (!contains(out, "20") || !contains(out, "<placeholder>")) fail("rigmor list missing placeholder row");
    if (!contains(out, "30") || !contains(out, "gamma")) fail("rigmor list missing id=30 gamma");
}

static void testListMissing() {
    seed_db_basic();
   
    std::string out;
    int rc = run_capture_stdout(rigmorExe, testDir, "list --missing", out);
    if (rc != 0) fail("rigmor list --missing returned non-zero");

    // Should contain placeholder row
    if (!contains(out, "<placeholder>")) fail("rigmor list --missing did not show placeholder");
    // Should NOT include other names
    if (contains(out, "alpha")) fail("rigmor list --missing incorrectly included non-missing alpha");
    if (contains(out, "gamma")) fail("rigmor list --missing incorrectly included non-missing gamma");
}

static void testFind()
{
    seed_db_basic();

    std::string out;
    int rc = run_capture_stdout(rigmorExe, testDir, "find 30", out);
    if (rc != 0) fail("rigmor find returned non-zero");

    if (!contains(out, "30") || !contains(out, "gamma"))
        fail("rigmor find did not print expected record");
}

static void testEdit()
{
    seed_db_basic();

    // sanity: name starts as alpha
    if (rigdb_name_for_id(db, 10) != "alpha")
        fail("seed broken: id=10 should be alpha");

    std::string out;
    int rc = run_capture_stdout(rigmorExe, testDir, "edit 10 ZZZ", out);
    if (rc != 0) fail("rigmor edit returned non-zero");

    // validate file got updated
    const std::string name = rigdb_name_for_id(db, 10);
    if (name != "ZZZ") fail("rigmor edit did not update name in db");

    // validate size unchanged and still aligned
    if (rigdb_count(db) != 3) fail("rigmor edit changed record count (should not)");
}

static void testDeleteSingle()
{
    seed_db_delete_case(); // 10..15

    std::string out;
    int rc = run_capture_stdout(rigmorExe, testDir, "delete 12", out);
    if (rc != 0) fail("rigmor delete single returned non-zero");

    if (rigdb_count(db) != 5) fail("delete single should reduce record count by 1");
    if (rigdb_has_id(db, 12)) fail("delete single did not remove id=12");
    if (!rigdb_has_id(db, 13)) fail("delete single removed wrong data (id=13 missing)");
}

static void testDeleteRange()
{
    seed_db_delete_case(); // 10..15

    std::string out;
    int rc = run_capture_stdout(rigmorExe, testDir, "delete 12 14", out);
    if (rc != 0) fail("rigmor delete range returned non-zero");

    if (rigdb_count(db) != 3) fail("delete range should reduce record count by 3");
    if (rigdb_has_id(db, 12)) fail("delete range did not remove id=12");
    if (rigdb_has_id(db, 13)) fail("delete range did not remove id=13");
    if (rigdb_has_id(db, 14)) fail("delete range did not remove id=14");

    if (!rigdb_has_id(db, 10)) fail("delete range removed id=10 unexpectedly");
    if (!rigdb_has_id(db, 11)) fail("delete range removed id=11 unexpectedly");
    if (!rigdb_has_id(db, 15)) fail("delete range removed id=15 unexpectedly");
}

static void testDeleteRangeWithGaps()
{
    AtlasRegionDisk regions[5] = {
        mk(10, "a"),
        mk(11, "b"),
        mk(13, "c"),   // missing 12
        mk(100, "d"),
        mk(101, "e"),
    };
    write_rigdb(db, regions, 5);
    if (!(rigdb_count(db) == 5)) fail("No elements should have been deleted");

    std::string out;
    int rc = run_capture_stdout(rigmorExe, testDir, "delete 11 13", out, true);
    if (rc == 0) fail("delete 11 13 should fail when ids are not contiguous");
}

static void testScan()
{
    seed_db_empty(); // ensure db exists and is empty (valid)

    std::string out;
    int rc = run_capture_stdout(rigmorExe, testDir, "scan", out);
    if (rc != 0) fail("rigmor scan returned non-zero");

    // scan should create some records for a real atlas
    const std::size_t count = rigdb_count(db);
    if (count == 0) fail("rigmor scan produced empty db (unexpected for test atlas)");
}

// --------------------------------------------------------
// ENTRYPOINT
// --------------------------------------------------------

int main(int argc, char** argv)
{
    try {
        if (argc != 3) throw std::runtime_error("usage: rigmor_e2e.exe <rigmor_exe_path> <test_dir>");
    
        rigmorExe = argv[1];
        testDir = argv[2];
        db = testDir / "test_atlas.rigdb";
        png = testDir / "test_atlas.png";
        cfg = testDir / "rigmor.config";
    
        std::error_code ec;
        std::filesystem::create_directories(testDir, ec);
        if (ec) throw std::runtime_error("Failed to create test dir");
    
        // Write a known rigdb
        // Include one placeholder name for --missing behavior.
        const char DEFAULT_ATLAS_NAME[32] = "<placeholder>";
        std::vector<AtlasRegionDisk> regions = {
            mk(10, "alpha"),
            mk(20, DEFAULT_ATLAS_NAME),
            mk(30, "gamma"),
        };
        write_rigdb(db, regions.data(), 50);
    
        // Write config (relative paths, because CWD will be testDir)
        write_text_file(cfg,
            std::string("db=") + db.filename().string() + "\n" +
            std::string("png=") + png.filename().string() + "\n"
        );

        // Copy file to test folder
        std::filesystem::copy_file("rigmor/test_atlas.png", png);
    
        if (!std::filesystem::exists(db)) throw std::runtime_error("failed to start - couldn't find db");
        if (!std::filesystem::exists(png)) throw std::runtime_error("failed to start - couldn't find png");
        if (!std::filesystem::exists(cfg)) throw std::runtime_error("failed to start - couldn't find cfg");
        if (!std::filesystem::exists(rigmorExe)) throw std::runtime_error("failed to start - couldn't find rigmorExe");
        if (!std::filesystem::exists(testDir)) throw std::runtime_error("failed to start - couldn't find testDir");
    
        testList();
        testListMissing();
        testFind();
        testEdit();
        testDeleteSingle();
        testDeleteRange();
        testDeleteRangeWithGaps();
        testScan();
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
         return 1;
    }

    fprintf(stdout, "ALL E2E TESTS PASSED\n");
    return 0;
}
