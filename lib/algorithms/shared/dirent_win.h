// lib/common/dirent_win.h — 最小 Windows dirent 兼容(opendir/readdir/closedir)
// 仅供 MSVC 使用: MSVC 无 <dirent.h>。POSIX 语义子集, 足够 hips_properties.cpp 的目录遍历。
#ifndef ACS_DIRENT_WIN_H
#define ACS_DIRENT_WIN_H

#if defined(_WIN32)

#include <windows.h>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>

struct dirent {
    char d_name[260];
};

struct _acs_dir_win {
    HANDLE h;
    WIN32_FIND_DATAA ffd;
    dirent ent;
    bool first;
};

typedef _acs_dir_win DIR;

// 以 * 通配列出目录: FindFirstFileA 首个结果存于 ffd, 后续 readdir 用 FindNextFileA
static inline DIR* opendir(const char* path) {
    char pattern[1024];
    std::snprintf(pattern, sizeof(pattern), "%s\\*", path ? path : ".");
    _acs_dir_win* d = new (std::nothrow) _acs_dir_win();
    if (!d) return nullptr;
    d->h = FindFirstFileA(pattern, &d->ffd);
    d->first = true;
    if (d->h == INVALID_HANDLE_VALUE) {
        delete d;
        return nullptr;
    }
    return d;
}

static inline const dirent* readdir(DIR* d) {
    if (!d) return nullptr;
    if (d->first) {
        d->first = false;
    } else {
        if (!FindNextFileA(d->h, &d->ffd)) return nullptr;
    }
    std::strncpy(d->ent.d_name, d->ffd.cFileName, sizeof(d->ent.d_name) - 1);
    d->ent.d_name[sizeof(d->ent.d_name) - 1] = '\0';
    return &d->ent;
}

static inline int closedir(DIR* d) {
    if (!d) return 0;
    if (d->h != INVALID_HANDLE_VALUE) FindClose(d->h);
    delete d;
    return 0;
}

#endif  // _WIN32
#endif  // ACS_DIRENT_WIN_H
