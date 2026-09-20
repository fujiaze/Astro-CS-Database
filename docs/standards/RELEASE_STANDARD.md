# AstroCS Release Standard

- 版本记录：**版本唯一源 = 根 `VERSION`**（`docs/governance/VERSION_NAMESPACES.md`），
  发布状态记录 = `docs/owner/RELEASE_STATUS.md`。
  > ⚠ **DOC-202 S12-Y2 订正（2026-09-20）**：原文要求「`CHANGELOG.md` + `docs/RELEASE_STATUS.md` 同步」，
  > 但 **`CHANGELOG.md` 在仓库中不存在**（义务悬空）。按「不得新增未登记的根目录条目」
  > （`AGENTS.md` §6 / `ENGINEERING_SPEC.md` §7：新根目录条目须先登记并经负责人确认）
  > ⇒ **撤下该要求**，版本变更历史由 **`VERSION` + git 历史** 承载。
  > 若负责人决定恢复 `CHANGELOG.md`，须先登记根目录条目再改回本行。
- 交付包：AstroCS_Review_<主题>_<YYYYMMDD>.zip，SHA256SUMS.txt。
- 包内容：README、reports/、evidence/、self_review/、
  source/full_first_party_after.zip + manifest、docs_snapshot/。
- 禁止打包 build/vendor/data；source archive 仅 first-party。
- Gate 字面量如实：PRE_RELEASE_ENGINEERING_FOUNDATION=PASS 仅当
  AUTHORITATIVE_DOC_CHAIN/SCIENCE/ALGORITHM/ARCHITECTURE/
  IMPLEMENTATION_STANDARDS 全 PASS；FINAL_REAL_DATA_VALIDATION=PENDING
  如实标注，不得冒充完成。
- 禁止删除清单：ACR 控制包/审核包、外部数据目录。
