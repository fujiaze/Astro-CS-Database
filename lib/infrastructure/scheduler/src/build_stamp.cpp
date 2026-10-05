// RUN-PROVENANCE-01: 构建期指纹的**唯一**消费 TU（全仓只此一处 include 生成头）。
// 生成头由根 CMakeLists.txt 的 add_custom_command 在**构建期**经
// eng/tools/gen_build_stamp.py 生成（依赖面 = git 索引给出的显式源文件清单）。
// 语义权威: docs/engineering/build/RELEASE.md「构建指纹合同」。
#include "acsd/core/build_stamp.h"

#include "build_stamp_generated.h"

namespace acsd::core {

const BuildStamp& build_stamp() {
  // 编译期常量 ⇒ 函数级 static 无竞态（C++11 起初始化线程安全）。
  static const BuildStamp kStamp{ACSD_BUILD_HEAD_SHA,
                                 ACSD_BUILD_DIRTY != 0,
                                 ACSD_BUILD_SOURCE_DIGEST,
                                 ACSD_CONFIGURE_HEAD_SHA,
                                 ACSD_BUILD_STAMP_UTC};
  return kStamp;
}

}  // namespace acsd::core
