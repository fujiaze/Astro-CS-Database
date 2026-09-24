# Astro Celestial Sphere Database（ACSD） Release Standard

> 上游：ASTROCS_DESIGN.md §8.4（模块与 ABI）

- 版本记录：**版本唯一源 = 根 `VERSION`**（`docs/owner/RELEASE_STATUS.md` §2），
  发布状态记录 = `docs/owner/RELEASE_STATUS.md`；版本变更历史由 **`VERSION` + git 历史** 承载。
  根目录条目须先登记并经负责人确认（`AGENTS.md` §6 / `ENGINEERING_SPEC.md` §7）。
- 交付包：AstroCS_Review_<主题>_<YYYYMMDD>.zip，SHA256SUMS.txt。
- 包内容：README、reports/、evidence/、self_review/、
  source/full_first_party_after.zip + manifest、docs_snapshot/。
- 禁止打包 build/vendor/data；source archive 仅 first-party。
- Gate 字面量如实：PRE_RELEASE_ENGINEERING_FOUNDATION=PASS 仅当
  AUTHORITATIVE_DOC_CHAIN/SCIENCE/ALGORITHM/ARCHITECTURE/
  IMPLEMENTATION_STANDARDS 全 PASS；FINAL_REAL_DATA_VALIDATION=PENDING
  如实标注，不得冒充完成。
- 禁止删除清单：ACR 控制包/审核包、外部数据目录。
