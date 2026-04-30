#ifndef RAGBOT_COMPAT_SHA_H
#define RAGBOT_COMPAT_SHA_H

#include "Types.h"

namespace rb {

// SHA-256 hex digest (lowercase, 64 chars).
auto sha256_hex(const Bytes& data) -> String;

// Convenience: hash an entire file by path. Returns empty on read failure.
auto sha256_file_hex(const String& path) -> String;

}  // namespace rb

#endif
