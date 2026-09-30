// lib/algorithms/shared/dirent_win.h — 最小 Windows dirent 兼容(opendir/readdir/closedir)
// 仅供 MSVC 使用: MSVC 无 <dirent.h>。POSIX 语义子集。
// 现状: 全仓**零 include 点**（原消费方 hips_properties.cpp 已改为经 aio 读, 不再自持
// 目录遍历）⇒ 本头当前无使用者, 属待清理的遗留件; 保留仅作 MSVC 目录遍历 shim 备查。
// 已知与 POSIX 侧的不等价（复用前须知）:
//   · 枚举顺序 = FindFirstFileA/FindNextFileA 返回序, **不排序** ⇒ 需确定性的调用方
//     必须自行 std::sort（POSIX readdir 同样无序, 两侧都需排序）;
//   · path 经 1024 字节固定缓冲 snprintf 截断, 截断后仍可能命中另一个存在的目录
//     （POSIX opendir 无此截断）;
//   · opendir(nullptr) 静默取 "."（POSIX 侧为未定义行为）;
//   · readdir 返回 const dirent*（POSIX 为 dirent*）;
//   · 守卫用 _WIN32 而非 _MSC_VER: MinGW-w64 亦定义 _WIN32, 与其自带 <dirent.h>
//     的 struct dirent 重复定义 ⇒ 该 shim 只对 MSVC 安全。
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
