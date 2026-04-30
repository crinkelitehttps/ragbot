#include "Io.h"
#include "Strings.h"

#ifdef RAGBOT_USE_QT
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#else
#include <filesystem>
#include <fstream>
#include <iterator>
#endif

namespace rb {

auto read_file_text(const String& path, bool* ok) -> String
{
#ifdef RAGBOT_USE_QT
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (ok != nullptr) *ok = false;
        return {};
    }
    if (ok != nullptr) *ok = true;
    return QString::fromUtf8(f.readAll());
#else
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (ok != nullptr) *ok = false;
        return {};
    }
    std::string out((std::istreambuf_iterator<char>(in)),
                    std::istreambuf_iterator<char>());
    if (ok != nullptr) *ok = true;
    return out;
#endif
}

auto read_file_bytes(const String& path, bool* ok) -> Bytes
{
#ifdef RAGBOT_USE_QT
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (ok != nullptr) *ok = false;
        return {};
    }
    if (ok != nullptr) *ok = true;
    return f.readAll();
#else
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (ok != nullptr) *ok = false;
        return {};
    }
    Bytes out((std::istreambuf_iterator<char>(in)),
              std::istreambuf_iterator<char>());
    if (ok != nullptr) *ok = true;
    return out;
#endif
}

auto write_file_text(const String& path, const String& content) -> bool
{
#ifdef RAGBOT_USE_QT
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const QByteArray bytes = content.toUtf8();
    return f.write(bytes) == bytes.size();
#else
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return out.good();
#endif
}

auto iter_files_recursive(const String& dir, const String& extension) -> Vector<String>
{
    Vector<String> out;
#ifdef RAGBOT_USE_QT
    // QDirIterator wants "*.json" pattern, not ".json".
    QString pattern = extension.startsWith(QLatin1Char('.'))
        ? QStringLiteral("*") + extension
        : extension;
    QDirIterator it(dir, { pattern }, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) out.push_back(it.next());
#else
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(dir, ec)) return out;
    for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension().string() == extension) {
            out.push_back(entry.path().string());
        }
    }
#endif
    return out;
}

auto path_filename(const String& path) -> String
{
#ifdef RAGBOT_USE_QT
    return QFileInfo(path).fileName();
#else
    return std::filesystem::path(path).filename().string();
#endif
}

auto path_exists(const String& path) -> bool
{
#ifdef RAGBOT_USE_QT
    return QFileInfo::exists(path);
#else
    std::error_code ec;
    return std::filesystem::exists(path, ec);
#endif
}

}  // namespace rb
