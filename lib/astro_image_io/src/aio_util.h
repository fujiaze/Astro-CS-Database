#ifndef AIO_UTIL_H
#define AIO_UTIL_H

#include <cstdio>
#include <string>

#ifdef _WIN32
#include <windows.h>

inline FILE* aio_fopen_utf8(const char* path, const char* mode) {
    int wpath_len = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    if (wpath_len <= 0) return std::fopen(path, mode);
    std::wstring wpath(wpath_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path, -1, &wpath[0], wpath_len);

    int wmode_len = MultiByteToWideChar(CP_UTF8, 0, mode, -1, nullptr, 0);
    std::wstring wmode(wmode_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, &wmode[0], wmode_len);

    return _wfopen(wpath.c_str(), wmode.c_str());
}
#else
inline FILE* aio_fopen_utf8(const char* path, const char* mode) {
    return std::fopen(path, mode);
}
#endif

#endif
