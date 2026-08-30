// IO-001 Artifact 事务 + FileIoAdapter 实现
#include "astrocs/io/io_adapter.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <system_error>

#ifndef _WIN32
#include <unistd.h>
#else
#include <process.h>
#define getpid _getpid
#endif

namespace astrocs::io {

namespace {
std::string err_msg(const std::string& what, const std::string& path) {
  return what + ": " + path + ": " + std::strerror(errno);
}
uint64_t fnv_update(uint64_t h, const char* data, size_t n) {
  for (size_t i = 0; i < n; ++i) { h ^= (unsigned char)data[i]; h *= 1099511628211ULL; }
  return h;
}
}  // namespace

Result<std::string> ArtifactTransaction::begin(const std::string& target_path) {
  if (active_) {
    return Result<std::string>::fail(Error(ErrorDomain::IO, "transaction already active"));
  }
  if (target_path.empty()) {
    return Result<std::string>::fail(Error(ErrorDomain::IO, "empty target path"));
  }
  // 同目录临时文件: <target>.tmp.<pid>.<seq>
  static uint64_t seq = 0;
  tmp_ = target_path + ".tmp." + std::to_string(static_cast<long>(getpid())) + "." +
         std::to_string(++seq);
  std::ofstream f(tmp_, std::ios::binary | std::ios::trunc);
  if (!f.is_open()) {
    std::string e = err_msg("cannot create temp file", tmp_);
    tmp_.clear();
    return Result<std::string>::fail(Error(ErrorDomain::IO, e));
  }
  f.close();
  target_ = target_path;
  written_ = 0;
  checksum_ = 1469598103934665603ULL;
  active_ = true;
  return Result<std::string>::ok(tmp_);
}

void ArtifactTransaction::write(const char* data, size_t n) {
  if (!active_) return;
  std::ofstream f(tmp_, std::ios::binary | std::ios::app);
  if (f.is_open()) {
    f.write(data, static_cast<std::streamsize>(n));
    f.close();
    written_ += n;
    checksum_ = fnv_update(checksum_, data, n);
  }
}

Result<void> ArtifactTransaction::commit() {
  if (!active_) {
    return Result<void>::fail(Error(ErrorDomain::IO, "commit without begin"));
  }
  // close 已完成 (write 关闭); verify: 读回长度 + 校验
  std::ifstream f(tmp_, std::ios::binary | std::ios::ate);
  if (!f.is_open()) {
    std::string e = err_msg("verify open failed", tmp_);
    abort();
    return Result<void>::fail(Error(ErrorDomain::IO, e));
  }
  std::streamoff sz = f.tellg();
  f.close();
  if (sz < 0 || static_cast<uint64_t>(sz) != written_) {
    std::string e = "verify length mismatch tmp=" + std::to_string(sz) +
                    " expected=" + std::to_string(written_);
    abort();
    return Result<void>::fail(Error(ErrorDomain::IO, e));
  }
  // rename (原子; Windows 失败给确定错误)
  if (std::rename(tmp_.c_str(), target_.c_str()) != 0) {
    std::string e = err_msg("rename failed (Windows: target may exist / locked)", target_);
    abort();
    return Result<void>::fail(Error(ErrorDomain::IO, e));
  }
  active_ = false;
  tmp_.clear();
  return Result<void>::success();
}

void ArtifactTransaction::abort() {
  if (active_ && !tmp_.empty()) std::remove(tmp_.c_str());
  active_ = false;
  tmp_.clear();
}

// ── FileIoAdapter ──
Result<std::string> FileIoAdapter::read_text(const std::string& path) const {
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open()) {
    return Result<std::string>::fail(Error(ErrorDomain::IO,
        err_msg("read_text open failed", path)));
  }
  std::ostringstream ss;
  ss << f.rdbuf();
  return Result<std::string>::ok(ss.str());
}

Result<void> FileIoAdapter::write_text(const std::string& path,
                                       const std::string& content) const {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f.is_open()) {
    return Result<void>::fail(Error(ErrorDomain::IO,
        err_msg("write_text open failed", path)));
  }
  f.write(content.data(), static_cast<std::streamsize>(content.size()));
  f.close();
  if (!f) {
    return Result<void>::fail(Error(ErrorDomain::IO,
        err_msg("write_text failed", path)));
  }
  return Result<void>::success();
}

Result<void> FileIoAdapter::read_bytes(const std::string& path,
                                       std::vector<uint8_t>* out) const {
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open()) {
    return Result<void>::fail(Error(ErrorDomain::IO,
        err_msg("read_bytes open failed", path)));
  }
  f.seekg(0, std::ios::end);
  std::streamoff sz = f.tellg();
  f.seekg(0, std::ios::beg);
  if (out) {
    out->resize(static_cast<size_t>(sz));
    if (sz > 0) f.read(reinterpret_cast<char*>(out->data()), sz);
  }
  return Result<void>::success();
}

bool FileIoAdapter::exists(const std::string& path) const {
  std::ifstream f(path, std::ios::binary);
  return f.is_open();
}

Result<void> FileIoAdapter::atomic_write(const std::string& path,
                                         const std::string& content) const {
  ArtifactTransaction tx;
  auto b = tx.begin(path);
  if (b.failed()) return Result<void>::fail(b.error());
  tx.write(content.data(), content.size());
  return tx.commit();
}

}  // namespace astrocs::io
