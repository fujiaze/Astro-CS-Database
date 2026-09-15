/* v6_atomic_publish.cpp — 原子发布实现（POSIX；Windows 分支保留）。 */
#include "astro/aio/v6_atomic_publish.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "astro/aio/v6_sha256.h"

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#include <windows.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace astrocs {
namespace aio {
namespace {

std::atomic<unsigned long> g_tmp_counter{0};

bool cancelled(const CancelFn& c) { return static_cast<bool>(c) && c(); }

long process_id() {
#if defined(_WIN32)
  return static_cast<long>(::_getpid());
#else
  return static_cast<long>(::getpid());
#endif
}

void close_fd(int fd) {
#if defined(_WIN32)
  ::_close(fd);
#else
  ::close(fd);
#endif
}

void unlink_path(const std::string& p) {
#if defined(_WIN32)
  ::_unlink(p.c_str());
#else
  ::unlink(p.c_str());
#endif
}

std::string dirname_of(const std::string& path) {
  const std::size_t p = path.find_last_of('/');
  if (p == std::string::npos) return ".";
  if (p == 0) return "/";
  return path.substr(0, p);
}

std::string basename_of(const std::string& path) {
  const std::size_t p = path.find_last_of('/');
  return (p == std::string::npos) ? path : path.substr(p + 1);
}

std::string nonce() {
  std::random_device rd;
  char buf[24];
  std::snprintf(buf, sizeof(buf), "%08x", static_cast<unsigned>(rd()));
  return std::string(buf);
}

std::string make_sidecar(const std::string& target, const char* tag) {
  const unsigned long seq = g_tmp_counter.fetch_add(1);
  return target + tag + std::to_string(process_id()) + "-" +
         std::to_string(seq) + "-" + nonce();
}

bool path_exists(const std::string& p) {
#if defined(_WIN32)
  return _access(p.c_str(), 0) == 0;
#else
  struct stat st;
  return ::lstat(p.c_str(), &st) == 0;
#endif
}

bool make_parent_dirs(const std::string& path) {
  std::string cur;
  std::size_t i = 0;
  if (!path.empty() && path[0] == '/') {
    cur = "/";
    i = 1;
  }
  while (i <= path.size()) {
    const std::size_t slash = path.find('/', i);
    const std::string seg =
        path.substr(i, slash == std::string::npos ? std::string::npos : slash - i);
    if (!seg.empty()) {
      if (!cur.empty() && cur.back() != '/') cur += "/";
      cur += seg;
#if defined(_WIN32)
      _mkdir(cur.c_str());
#else
      ::mkdir(cur.c_str(), 0755);
#endif
    }
    if (slash == std::string::npos) break;
    i = slash + 1;
  }
  return true;
}

bool fsync_dir(const std::string& path, std::string* err) {
#if defined(_WIN32)
  (void)path;
  (void)err;
  return true;
#else
  const int fd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY);
  if (fd < 0) {
    if (err) *err = std::string("open dir failed: ") + std::strerror(errno);
    return false;
  }
  const int rc = ::fsync(fd);
  const int saved = errno;
  close_fd(fd);
  if (rc != 0 && saved != EINVAL && saved != ENOTSUP) {
    if (err) *err = std::string("fsync dir failed: ") + std::strerror(saved);
    return false;
  }
  return true;
#endif
}

}  // namespace

const char* publish_status_name(PublishStatus s) {
  switch (s) {
    case PublishStatus::kOk: return "OK";
    case PublishStatus::kErrParam: return "ERR_PARAM";
    case PublishStatus::kErrIo: return "ERR_IO";
    case PublishStatus::kErrCancelled: return "ERR_CANCELLED";
    case PublishStatus::kErrState: return "ERR_STATE";
    case PublishStatus::kErrDiskfull: return "ERR_DISKFULL";
    case PublishStatus::kErrChecksum: return "ERR_CHECKSUM";
    case PublishStatus::kErrMismatch: return "ERR_MISMATCH";
    case PublishStatus::kErrInternal: return "ERR_INTERNAL";
    default: return "ERR_OTHER";
  }
}

bool write_all_fd(int fd, const void* data, std::size_t len,
                  const CancelFn& cancel, std::string* err) {
  const char* p = static_cast<const char*>(data);
  std::size_t left = len;
  while (left > 0) {
    if (cancelled(cancel)) {
      if (err) *err = "cancelled";
      return false;
    }
#if defined(_WIN32)
    const unsigned chunk = static_cast<unsigned>(left > (1u << 20) ? (1u << 20) : left);
    const int w = _write(fd, p, chunk);
#else
    const ssize_t w = ::write(fd, p, left);
#endif
    if (w < 0) {
#if !defined(_WIN32)
      if (errno == EINTR) continue;
      if (err) *err = std::string("write failed: ") + std::strerror(errno);
#endif
      return false;
    }
    if (w == 0) {
      if (err) *err = "write returned 0";
      return false;
    }
    p += w;
    left -= static_cast<std::size_t>(w);
  }
  return true;
}

bool sha256_file_hex(const std::string& path, std::string* hex) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  Sha256 h;
  unsigned char buf[64 * 1024];
  std::size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) h.update(buf, n);
  const bool read_ok = (std::ferror(f) == 0);
  const bool close_ok = (std::fclose(f) == 0);
  if (!read_ok || !close_ok) return false;
  *hex = h.hex();
  return true;
}

bool remove_tree(const std::string& path) {
#if defined(_WIN32)
  (void)path;
  return false;
#else
  struct stat st;
  if (::lstat(path.c_str(), &st) != 0) return errno == ENOENT;
  if (!S_ISDIR(st.st_mode)) return ::unlink(path.c_str()) == 0;
  DIR* d = ::opendir(path.c_str());
  if (d == nullptr) return false;
  bool ok = true;
  while (struct dirent* e = ::readdir(d)) {
    const std::string name = e->d_name;
    if (name == "." || name == "..") continue;
    if (!remove_tree(path + "/" + name)) ok = false;
  }
  ::closedir(d);
  if (::rmdir(path.c_str()) != 0 && errno != ENOENT) ok = false;
  return ok;
#endif
}

bool fsync_tree(const std::string& path, std::uint64_t* n_files,
                std::string* err) {
#if defined(_WIN32)
  (void)path;
  (void)n_files;
  (void)err;
  return true;
#else
  struct stat st;
  if (::lstat(path.c_str(), &st) != 0) {
    if (err) *err = std::string("lstat failed: ") + std::strerror(errno);
    return false;
  }
  if (!S_ISDIR(st.st_mode)) {
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
      if (err) *err = std::string("open failed: ") + std::strerror(errno);
      return false;
    }
    const int rc = ::fsync(fd);
    const int saved = errno;
    close_fd(fd);
    if (rc != 0) {
      if (err) *err = std::string("fsync failed: ") + std::strerror(saved);
      return false;
    }
    if (n_files) *n_files += 1;
    return true;
  }
  DIR* d = ::opendir(path.c_str());
  if (d == nullptr) {
    if (err) *err = std::string("opendir failed: ") + std::strerror(errno);
    return false;
  }
  bool ok = true;
  std::string first_err;
  while (struct dirent* e = ::readdir(d)) {
    const std::string name = e->d_name;
    if (name == "." || name == "..") continue;
    std::string child_err;
    if (!fsync_tree(path + "/" + name, n_files, &child_err)) {
      ok = false;
      if (first_err.empty()) first_err = child_err;
    }
  }
  ::closedir(d);
  std::string derr;
  if (!fsync_dir(path, &derr)) {
    ok = false;
    if (first_err.empty()) first_err = derr;
  }
  if (!ok && err) *err = first_err;
  return ok;
#endif
}

namespace {

PublishStatus classify_fsync_errno(int e) {
#if defined(_WIN32)
  (void)e;
  return PublishStatus::kErrIo;
#else
  if (e == ENOSPC || e == EDQUOT) return PublishStatus::kErrDiskfull;
  return PublishStatus::kErrIo;
#endif
}

void cleanup_tmp(const std::string& tmp, PublishResult* res) {
  if (path_exists(tmp)) {
    unlink_path(tmp);
  }
  res->tmp_residue = path_exists(tmp);
}

}  // namespace

PublishResult atomic_publish_file(const std::string& target,
                                  const FileWriterFn& writer,
                                  const VerifyFn& verify,
                                  const PublishOptions& opts,
                                  const CancelFn& cancel) {
  PublishResult res;
  res.target = target;
  if (target.empty() || !writer) {
    res.status = PublishStatus::kErrParam;
    res.message = "empty target or null writer";
    return res;
  }
  if (cancelled(cancel)) {
    res.status = PublishStatus::kErrCancelled;
    res.message = "cancelled before start";
    return res;
  }
  const std::string tmp = make_sidecar(target, ".tmp-");
  res.tmp = tmp;
#if defined(_WIN32)
  int fd = -1;
  for (int attempt = 0; attempt < 8 && fd < 0; ++attempt) {
    fd = _open(tmp.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY, _S_IREAD | _S_IWRITE);
  }
#else
  int fd = -1;
  for (int attempt = 0; attempt < 8 && fd < 0; ++attempt) {
    fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd < 0 && errno != EEXIST) break;
  }
#endif
  if (fd < 0) {
    res.status = PublishStatus::kErrIo;
    res.message = std::string("open tmp failed: ") + std::strerror(errno);
    return res;
  }

  std::string werr;
  const bool writer_ok = writer(fd, cancel, &werr);
  if (!writer_ok || cancelled(cancel)) {
    close_fd(fd);
    cleanup_tmp(tmp, &res);
    res.status = cancelled(cancel) ? PublishStatus::kErrCancelled
                                   : PublishStatus::kErrIo;
    res.message = werr.empty() ? "writer failed" : werr;
    return res;
  }

#if defined(_WIN32)
  std::uint64_t size = 0;
  if (_commit(fd) != 0) {
    close_fd(fd);
    cleanup_tmp(tmp, &res);
    res.status = PublishStatus::kErrIo;
    res.message = "commit failed";
    return res;
  }
  size = _filelengthi64(fd);
#else
  if (::fsync(fd) != 0) {
    const int saved = errno;
    close_fd(fd);
    cleanup_tmp(tmp, &res);
    res.status = classify_fsync_errno(saved);
    res.message = std::string("fsync failed: ") + std::strerror(saved);
    return res;
  }
  struct stat st;
  std::uint64_t size = 0;
  if (::fstat(fd, &st) == 0) size = static_cast<std::uint64_t>(st.st_size);
#endif
  close_fd(fd);
  res.bytes_written = size;

  std::string sha;
  if (!sha256_file_hex(tmp, &sha)) {
    cleanup_tmp(tmp, &res);
    res.status = PublishStatus::kErrIo;
    res.message = "sha256 readback failed";
    return res;
  }
  res.sha256_hex = sha;

  if (cancelled(cancel)) {
    cleanup_tmp(tmp, &res);
    res.status = PublishStatus::kErrCancelled;
    res.message = "cancelled before rename";
    return res;
  }

#if defined(_WIN32)
  if (::remove(target.c_str()) != 0 && errno != ENOENT) { /* best effort */ }
#endif
  if (::rename(tmp.c_str(), target.c_str()) != 0) {
    const int saved = errno;
    cleanup_tmp(tmp, &res);
    res.status = (saved == EISDIR || saved == ENOTDIR || saved == ENOTEMPTY)
                     ? PublishStatus::kErrState
                     : PublishStatus::kErrIo;
    res.message = std::string("rename failed: ") + std::strerror(saved);
    return res;
  }
  res.renamed = true;

  if (opts.fsync_directory) {
    std::string derr;
    if (!fsync_dir(dirname_of(target), &derr)) {
      res.status = PublishStatus::kErrIo;
      res.message = derr;
      // 已 rename，但目录 fsync 失败：仍按失败上报（fail-closed）。
      return res;
    }
  }
  if (opts.verify_after_rename && verify) {
    std::string verr;
    if (!verify(target, &verr)) {
      if (opts.remove_on_verify_failure) {
        unlink_path(target);
        if (opts.fsync_directory) {
          std::string derr;
          fsync_dir(dirname_of(target), &derr);
        }
      }
      res.status = PublishStatus::kErrChecksum;
      res.message = verr.empty() ? "post-publish verification failed" : verr;
      return res;
    }
  }
  res.status = PublishStatus::kOk;
  return res;
}

PublishResult atomic_write_bytes(const std::string& target,
                                 const std::string& bytes,
                                 const VerifyFn& verify,
                                 const PublishOptions& opts,
                                 const CancelFn& cancel) {
  const FileWriterFn writer = [&bytes](int fd, const CancelFn& c, std::string* err) {
    return write_all_fd(fd, bytes.data(), bytes.size(), c, err);
  };
  return atomic_publish_file(target, writer, verify, opts, cancel);
}

PublishResult atomic_publish_directory(const std::string& target_dir,
                                       const DirBuilderFn& builder,
                                       const VerifyFn& verify,
                                       const PublishOptions& opts,
                                       const CancelFn& cancel) {
  PublishResult res;
  res.target = target_dir;
  if (target_dir.empty() || !builder) {
    res.status = PublishStatus::kErrParam;
    res.message = "empty target_dir or null builder";
    return res;
  }
  if (cancelled(cancel)) {
    res.status = PublishStatus::kErrCancelled;
    res.message = "cancelled before start";
    return res;
  }
#if defined(_WIN32)
  // 目录级原子发布（staging + rename 整树）在 Windows 上未实现：显式
  // fail-closed（返回 UNSUPPORTED），不得静默半发布。
  (void)builder;
  (void)verify;
  (void)opts;
  (void)cancel;
  res.status = PublishStatus::kErrUnsupported;
  res.message = "directory atomic publish not implemented on Windows in this revision";
  return res;
#else
  // 目标若非空目录 -> STATE（拒绝覆盖非空）。
  struct stat st;
  if (::lstat(target_dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
    DIR* d = ::opendir(target_dir.c_str());
    if (d != nullptr) {
      bool empty = true;
      while (struct dirent* e = ::readdir(d)) {
        const std::string n = e->d_name;
        if (n != "." && n != "..") {
          empty = false;
          break;
        }
      }
      ::closedir(d);
      if (!empty) {
        res.status = PublishStatus::kErrState;
        res.message = "target directory exists and is non-empty";
        return res;
      }
    }
  }
  const std::string parent = dirname_of(target_dir);
  const std::string base = basename_of(target_dir);
  const std::string staging =
      parent + "/." + base + ".staging.tmp-" +
      std::to_string(static_cast<long>(::getpid())) + "-" + nonce();
  res.tmp = staging;
  make_parent_dirs(parent);
  if (::mkdir(staging.c_str(), 0755) != 0) {
    res.status = PublishStatus::kErrIo;
    res.message = std::string("mkdir staging failed: ") + std::strerror(errno);
    return res;
  }

  std::string berr;
  const bool build_ok = builder(staging, cancel, &berr);
  if (!build_ok || cancelled(cancel)) {
    remove_tree(staging);
    res.status = cancelled(cancel) ? PublishStatus::kErrCancelled
                                   : PublishStatus::kErrIo;
    res.message = berr.empty() ? "builder failed" : berr;
    res.tmp_residue = path_exists(staging);
    return res;
  }
  std::uint64_t n_files = 0;
  if (!fsync_tree(staging, &n_files, &berr)) {
    remove_tree(staging);
    res.status = PublishStatus::kErrIo;
    res.message = berr;
    res.tmp_residue = path_exists(staging);
    return res;
  }
  if (cancelled(cancel)) {
    remove_tree(staging);
    res.status = PublishStatus::kErrCancelled;
    res.message = "cancelled before promote";
    res.tmp_residue = path_exists(staging);
    return res;
  }
  if (::rename(staging.c_str(), target_dir.c_str()) != 0) {
    const int saved = errno;
    remove_tree(staging);
    res.status = (saved == ENOTEMPTY || saved == EEXIST) ? PublishStatus::kErrState
                                                         : PublishStatus::kErrIo;
    res.message = std::string("rename staging failed: ") + std::strerror(saved);
    res.tmp_residue = path_exists(staging);
    return res;
  }
  res.renamed = true;
  res.bytes_written = n_files;
  if (opts.fsync_directory) {
    std::string derr;
    if (!fsync_dir(parent, &derr)) {
      res.status = PublishStatus::kErrIo;
      res.message = derr;
      return res;
    }
  }
  if (opts.verify_after_rename && verify) {
    std::string verr;
    if (!verify(target_dir, &verr)) {
      if (opts.remove_on_verify_failure) {
        remove_tree(target_dir);
        if (opts.fsync_directory) {
          std::string derr;
          fsync_dir(parent, &derr);
        }
      }
      res.status = PublishStatus::kErrChecksum;
      res.message = verr.empty() ? "post-publish verification failed" : verr;
      return res;
    }
  }
  res.status = PublishStatus::kOk;
  return res;
#endif  // !defined(_WIN32)
}

}  // namespace aio
}  // namespace astrocs
