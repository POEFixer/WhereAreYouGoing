// Unicode-safe whole-file read and atomic replace (temporary file, then
// MoveFileExW with MOVEFILE_REPLACE_EXISTING), so a crash or a full disk never
// leaves a half-written settings file behind. Never throws.
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <io.h>
#include <string>
#include <system_error>

namespace AtomicFile {

constexpr long long kMaxReadBytes = 8ll << 20;   // a settings file is never this large

// True when something exists at `path`, even if it cannot be opened right now
// (another program holds it, access denied). Only "not found" counts as absent,
// so a temporarily locked settings file is never mistaken for a first start.
inline bool Present(const std::filesystem::path& path) {
    if (::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return true;
    const DWORD err = ::GetLastError();
    return err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND;
}

inline bool ReadAll(const std::filesystem::path& path, std::string& out) {
    out.clear();
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return false;
    // An oversized file fails at once instead of after reading 8 MiB.
    if (_filelengthi64(_fileno(f)) > kMaxReadBytes) {
        std::fclose(f);
        return false;
    }
    char buf[8192];
    size_t n = 0;
    bool ok = true;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        out.append(buf, n);
        if (static_cast<long long>(out.size()) > kMaxReadBytes) { ok = false; break; }
    }
    if (std::ferror(f)) ok = false;
    std::fclose(f);
    if (!ok) out.clear();
    return ok;
}

inline bool WriteAll(const std::filesystem::path& path, const std::string& data) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::filesystem::path tmp = path;
    tmp += L".tmp";
    FILE* f = nullptr;
    if (_wfopen_s(&f, tmp.c_str(), L"wb") != 0 || !f) return false;
    const bool wrote = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    const bool flushed = std::fflush(f) == 0;
    std::fclose(f);
    if (!wrote || !flushed) {
        ::DeleteFileW(tmp.c_str());
        return false;
    }
    if (!::MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        ::DeleteFileW(tmp.c_str());
        return false;
    }
    return true;
}

inline bool AppendAll(const std::filesystem::path& path, const std::string& data) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"ab") != 0 || !f) return false;
    const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    std::fclose(f);
    return ok;
}

} // namespace AtomicFile
