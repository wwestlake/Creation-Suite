#pragma once

#include <string>

namespace creation::assistant::detail
{
// Lowercase hexadecimal SHA-256 of the bytes in `data`.
std::string sha256Hex(const std::string& data);
}
