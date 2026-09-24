// ACSD Core — 构建期指纹（RUN-PROVENANCE-01）
//
// 语义权威: docs/VERSIONING.md「构建指纹合同」；证据: run/RUN-PROVENANCE-01/REPORT.md。
//
// 为什么需要它：version_generated.h 里的 ASTROCS_COMMIT_SHA 由 **CMake configure 期**
// 的 git rev-parse HEAD 采样一次，而 Ninja 的 RERUN_CMAKE 规则只依赖 CMake 输入
// （实测 build/build.ninja:19586 的依赖面里没有任何 .cpp/.h）⇒ 改源码不会重跑
// configure ⇒ 编译进去的是新代码、provenance 记的却是旧 SHA。同一次 configure 下的
// 所有运行共享同一个 source_sha，于是"同 source_sha"被误当成"同一二进制"。
//
// 本接口给的是**构建期**采样、且由源集**内容**决定的指纹：
//   - source_digest() 是唯一可用于判"同一代码 / 同一二进制"的量（64hex）；
//   - head_sha() 只是构建时刻的 HEAD，会因纯文档提交前进 —— 不得单独当指纹用；
//   - dirty() 是构建时刻工作树是否有未提交改动（VER-001 §2 的 dirty 语义）；
//   - configure_head_sha() 是 configure 期 HEAD，仅供对照 source_sha 的来历。
//
// 实现唯一: lib/infrastructure/scheduler/src/build_stamp.cpp（全仓**唯一** include
// build_stamp_generated.h 的 TU）。这样生成头一变只重编译该小 TU，不级联大 TU。
#pragma once

#include <string>

namespace astrocs::core {

struct BuildStamp {
  std::string head_sha;            // 构建期 HEAD（40hex）
  bool dirty = false;              // 构建期工作树是否有未提交改动
  std::string source_digest;       // 源集内容摘要（64hex）—— "同一代码"的判据
  std::string configure_head_sha;  // CMake configure 期 HEAD（对照用，40hex）
  std::string stamp_utc;           // 构建期采样时刻（UTC，仅供人读，不进指纹）
};

// 进程级常量（编译期烙入）；返回引用，无分配、无 IO。
const BuildStamp& build_stamp();

}  // namespace astrocs::core
