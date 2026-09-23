// RUN-PROVENANCE-01: 构建期指纹的**唯一**消费 TU（全仓只此一处 include 生成头）。
// 生成头由根 CMakeLists.txt 的 add_custom_command 在**构建期**经
// eng/tools/gen_build_stamp.py 生成（依赖面 = git 索引给出的显式源文件清单）。
// 语义权威: docs/VERSIONING.md「构建指纹合同」。
#include "astrocs/core/build_stamp.h"

#include "build_stamp_generated.h"

namespace astrocs::core {

const BuildStamp& build_stamp() {
  // 编译期常量 ⇒ 函数级 static 无竞态（C++11 起初始化线程安全）。
  static const BuildStamp kStamp{ASTROCS_BUILD_HEAD_SHA,
                                 ASTROCS_BUILD_DIRTY != 0,
                                 ASTROCS_BUILD_SOURCE_DIGEST,
                                 ASTROCS_CONFIGURE_HEAD_SHA,
                                 ASTROCS_BUILD_STAMP_UTC};
  return kStamp;
}

}  // namespace astrocs::core
