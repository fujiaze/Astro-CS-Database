// lib/infrastructure/cli/export/export.h — export 子命令入口（CLI-001）
//
// export 是 §6.2 唯一命令树里的平级独立命令之一（ASTROCS_DESIGN §1.2）：
// 本入口只做参数解析、模板填充、预检与退出码；执行/校验/计划/检视全部委托
// 会话层（cli/commands.cpp 的 session_dispatch），**不含任何科学实现**（§6.1）。
// 本命令不调用、不依赖另外两个命令；阶段间只通过磁盘产品 + manifest + 哈希交换。
#pragma once

#include "../subcommand.h"

namespace astrocs::cli::cmd {

inline const Subcommand& export_subcommand() {
    static const Subcommand k = {"export", SESSION_EXPORT};
    return k;
}

}  // namespace astrocs::cli::cmd
