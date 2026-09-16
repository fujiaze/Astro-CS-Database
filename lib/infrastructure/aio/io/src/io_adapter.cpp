// IO-001 Artifact 事务 + FileIoAdapter 实现
#include "astrocs/io/io_adapter.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
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
  // seq 为函数级 static atomic: 并发 begin 于同进程同 pid 下也能生成唯一 tmp 名
  // (修复前 ++seq 数据竞争, 可能撞名导致两事务互踩临时文件)。
  static std::atomic<uint64_t> seq{0};
  tmp_ = target_path + ".tmp." + std::to_string(static_cast<long>(getpid())) + "." +
         std::to_string(seq.fetch_add(1, std::memory_order_relaxed) + 1);
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
    if (!f) {
      // 写失败 (含短写/短读) 不得静默吞掉: 锁存, commit 时按合同报确定错误
      // (修复前失败被忽略, commit 仍可能 rename 半截内容 → 假成功)。
      write_failed_ = true;
      return;
    }
    written_ += n;
    checksum_ = fnv_update(checksum_, data, n);
  } else {
    // tmp 重新打开失败: 同样视为写失败并锁存 (commit 时报错, 不带病前进)
    write_failed_ = true;
  }
}

Result<void> ArtifactTransaction::commit() {
  if (!active_) {
    return Result<void>::fail(Error(ErrorDomain::IO, "commit without begin"));
  }
  if (write_failed_) {
    // 合同: 任何写入失败 → 确定错误 + 清理现场, 绝不 rename 半截数据
    std::string e = "verify write failed (short write / reopen failed): " + tmp_;
    abort();
    return Result<void>::fail(Error(ErrorDomain::IO, e));
  }
  // close 已完成 (write 关闭); verify: 读回长度 + 全量重算校验和
  // (修复前只比长度不比校验和: 内容损坏时假成功)。分块流式读, 不整载入内存。
  std::ifstream f(tmp_, std::ios::ios_base::binary);
  if (!f.is_open()) {
    std::string e = err_msg("verify open failed", tmp_);
    abort();
    return Result<void>::fail(Error(ErrorDomain::IO, e));
  }
  uint64_t sz = 0;
  uint64_t got = 1469598103934665603ULL;
  char buf[65536];
  while (f.read(buf, static_cast<std::streamsize>(sizeof(buf))) || f.gcount() > 0) {
    got = fnv_update(got, buf, static_cast<size_t>(f.gcount()));
    sz += static_cast<uint64_t>(f.gcount());
    if (!f) break;  // 短读(尾部块或读错误)后本块已累计, 终止
  }
  if (f.bad()) {
    std::string e = err_msg("verify read failed", tmp_);
    f.close();
    abort();
    return Result<void>::fail(Error(ErrorDomain::IO, e));
  }
  f.close();
  if (sz != written_) {
    std::string e = "verify length mismatch tmp=" + std::to_string(sz) +
                    " expected=" + std::to_string(written_);
    abort();
    return Result<void>::fail(Error(ErrorDomain::IO, e));
  }
  if (got != checksum_) {
    std::string e = "verify checksum mismatch tmp=" + tmp_ +
                    " expected=" + std::to_string(checksum_) +
                    " got=" + std::to_string(got);
    abort();
    return Result<void>::fail(Error(ErrorDomain::IO, e));
  }
  // rename (原子替换; 覆盖已存在目标是 atomic_write 的核心用途):
  // UCRT std::rename 不替换已存在目标 (MOVEFILE 无 REPLACE_EXISTING), Windows
  // 覆盖写必失败; std::filesystem::rename 两侧语义一致 (POSIX 替换 / MSVC STL
  // 带 MOVEFILE_REPLACE_EXISTING)。失败给确定错误并保留事务现场 (abort 由调用方决定)。
  {
    std::error_code ec;
    std::filesystem::rename(tmp_, target_, ec);
    if (ec) {
      std::string e = "rename failed: " + target_ + ": " + ec.message();
      abort();
      return Result<void>::fail(Error(ErrorDomain::IO, e));
    }
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
