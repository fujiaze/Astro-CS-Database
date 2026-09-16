/* lib/astro_image_io/third_party/cfitsio/win_compat/unistd.h
 * MSVC 无 <unistd.h>。cfitsio 为纯 C, 用 _ 前缀 CRT 函数名映射 POSIX 名。
 * 仅供 cfitsio C 源使用(不改 C++)。
 */
#ifndef ACS_CFITSIO_UNISTD_H
#define ACS_CFITSIO_UNISTD_H

#if defined(_WIN32)

#include <io.h>
#include <process.h>
#include <direct.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef getcwd
#define getcwd _getcwd
#endif
#ifndef getpid
#define getpid _getpid
#endif
#ifndef getppid
#define getppid _getpid
#endif
#ifndef read
#define read _read
#endif
#ifndef write
#define write _write
#endif
#ifndef close
#define close _close
#endif
#ifndef unlink
#define unlink _unlink
#endif
#ifndef access
#define access _access
#endif
#ifndef ftruncate
#define ftruncate _chsize
#endif
#ifndef fsync
#define fsync _commit
#endif
#ifndef isatty
#define isatty _isatty
#endif

#endif  /* _WIN32 */
#endif  /* ACS_CFITSIO_UNISTD_H */
