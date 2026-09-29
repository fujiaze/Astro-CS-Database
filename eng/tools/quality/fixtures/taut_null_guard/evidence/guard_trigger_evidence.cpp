// 证据 TU（Linux 树上自证）: 真编译真运行，证明「守卫失败路径」可被触发。
//
// 口径: 判据的静态面只能证明「守卫不再是编译期恒真」，还必须有一段**可执行**
// 证据证明那条失败路径真的会被走到（恒真门没有证据资格）。本 TU 驱动三态:
//   正例  : 故障名合法 + 条件成立   ⇒ 0 失败（行为与改写前一致）
//   负例  : 故障名 = nullptr         ⇒ 1 失败（失败路径真的被走到）
//   负例2 : 故障名 = ""（空串）      ⇒ 1 失败
//   恢复  : 换回合法故障名           ⇒ 0 失败（证明修复不是恒假门）
// 任一态不成立 ⇒ rc=1，证据步判红。
#include "taut_checks.hpp"

#include <cstdio>
#include <cstring>

namespace {

int evidence_main() {
    // 态 1 · 正例（合法故障名，条件成立）
    {
        fixtaut::CheckState cs;
        FIXTAUT_CHECK(cs, 1 == 1, "restore_case");
        if (cs.failures != 0 || cs.fault_reported) {
            std::fprintf(stderr, "EVIDENCE_FAIL positive failures=%d reported=%d\n",
                         cs.failures, static_cast<int>(cs.fault_reported));
            return 1;
        }
        std::printf("EVIDENCE positive ok (failures=0)\n");
    }
    // 态 2 · 负例（故障名缺失 ⇒ 旧恒真守卫会静默跳过，此处必须失败）
    {
        fixtaut::CheckState cs;
        FIXTAUT_CHECK(cs, 1 == 1, nullptr);
        if (cs.failures != 1 || cs.fault_reported) {
            std::fprintf(stderr, "EVIDENCE_FAIL null_faultname failures=%d reported=%d\n",
                         cs.failures, static_cast<int>(cs.fault_reported));
            return 1;
        }
        std::printf("EVIDENCE null_faultname ok (failures=1, failure path reached)\n");
    }
    // 态 3 · 负例（故障名为空串）
    {
        fixtaut::CheckState cs;
        FIXTAUT_CHECK(cs, 1 == 1, "");
        if (cs.failures != 1 || cs.fault_reported) {
            std::fprintf(stderr, "EVIDENCE_FAIL empty_faultname failures=%d\n", cs.failures);
            return 1;
        }
        std::printf("EVIDENCE empty_faultname ok (failures=1, failure path reached)\n");
    }
    // 态 4 · 负例不得被当成注入名吞掉（非法名不参与注入，但必须计失败）
    {
        fixtaut::CheckState cs;
        fixtaut::FaultRegistry::instance().active.push_back("");
        FIXTAUT_CHECK(cs, 1 == 1, "");
        if (cs.fault_reported) {
            std::fprintf(stderr, "EVIDENCE_FAIL illegal name was injected\n");
            return 1;
        }
        std::printf("EVIDENCE illegal_name_not_injected ok\n");
    }
    // 态 5 · 恢复（合法名: 未激活 ⇒ 0 失败；已激活 ⇒ 走注入分支 1 失败）
    {
        fixtaut::CheckState cs;
        FIXTAUT_CHECK(cs, 1 == 1, "restore_case");
        if (cs.failures != 0) {
            std::fprintf(stderr, "EVIDENCE_FAIL restored failures=%d\n", cs.failures);
            return 1;
        }
        fixtaut::FaultRegistry::instance().active.push_back("live_fault");
        fixtaut::CheckState cs2;
        FIXTAUT_CHECK(cs2, 1 == 1, "live_fault");
        if (cs2.failures != 1 || !cs2.fault_reported) {
            std::fprintf(stderr, "EVIDENCE_FAIL live injection failures=%d reported=%d\n",
                         cs2.failures, static_cast<int>(cs2.fault_reported));
            return 1;
        }
        std::printf("EVIDENCE restored ok (0 failures without injection, 1 with)\n");
    }
    return 0;
}

}  // namespace

int main() { return evidence_main(); }
