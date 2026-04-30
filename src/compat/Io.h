#ifndef RAGBOT_COMPAT_IO_H
#define RAGBOT_COMPAT_IO_H

#include "Types.h"

namespace rb {

// File I/O. Returns empty + sets *ok=false on failure when ok is provided.
auto read_file_text(const String& path, bool* ok = nullptr) -> String;
auto read_file_bytes(const String& path, bool* ok = nullptr) -> Bytes;
auto write_file_text(const String& path, const String& content) -> bool;

// Recursive scan, returns absolute paths whose extension equals `extension`
// (e.g. ".json"). Comparison is case-sensitive on the extension argument.
auto iter_files_recursive(const String& dir, const String& extension) -> Vector<String>;

// Path helpers.
auto path_filename(const String& path) -> String;   // basename
auto path_exists(const String& path) -> bool;

}  // namespace rb

#endif
