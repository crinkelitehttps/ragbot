#include "Io.h"
#include "Strings.h"

#include <filesystem>
#include <fstream>
#include <iterator>

namespace rb {

auto read_file_text(const String& path, bool* ok) -> String
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (ok != nullptr) *ok = false;
        return {};
    }
    std::string out((std::istreambuf_iterator<char>(in)),
                    std::istreambuf_iterator<char>());
    if (ok != nullptr) *ok = true;
    return out;
}

auto read_file_bytes(const String& path, bool* ok) -> Bytes
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (ok != nullptr) *ok = false;
        return {};
    }
    Bytes out((std::istreambuf_iterator<char>(in)),
              std::istreambuf_iterator<char>());
    if (ok != nullptr) *ok = true;
    return out;
}

auto write_file_text(const String& path, const String& content) -> bool
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return out.good();
}

auto iter_files_recursive(const String& dir, const String& extension) -> Vector<String>
{
    Vector<String> out;
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(dir, ec)) return out;
    for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension().string() == extension)
            out.push_back(entry.path().string());
    }
    return out;
}

auto path_filename(const String& path) -> String
{
    return std::filesystem::path(path).filename().string();
}

auto path_exists(const String& path) -> bool
{
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

}  // namespace rb
