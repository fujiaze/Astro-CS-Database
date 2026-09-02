// WIN-001: cfitsio 个别 .c (eval_l.c) include <unistd.h> (POSIX); MSVC 无此头。
// 空 shim — cfitsio 仅用它声明 unlink/access 等, 实际未调用这些分支 (Windows 驱动不同)。
#pragma once
#ifndef _WIN32
#error "win32_pthread_shim/unistd.h is MSVC/Windows only"
#endif
