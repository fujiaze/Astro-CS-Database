# QA-V19R7-A1-02 Evidence Index — 文件审计与标准扫描（M+E）

- task: QA-V19R7-A1-02
- gate: G-QA (A阶段)
- status: DONE
- date: 2026-08-22
- 模式: 替代 file_audit（tools/file_audit 缺失 → find+wc + grep 扫描）

## 通过条件
- 审计覆盖 700+ 文件（目标 713）
- 产出 `file_audit_before.json` + `standards_violations.json` + `audit_stats.json`
- 违规可定位到文件/行

## 实际结果
- lib/ 文件总数: 873（覆盖 873/713，含 tests/docs/CMake 等全量文件；按模块分组已落盘）
- 按模块分组: acr 216 / astro_image_io 260 / healpix_db 145 / phase2 46 / plate_solve 44 / photometric_calib 43 / orchestrator 43 / calibration 18 / star_detector 16 / common 14 / dynamic_psf 11 / snr_estimator 10 / gaia_xpsd_client 7
- TRACEABILITY: 64 行声明 → 实测 63 数据行（去表头），broken 0（实现文件/symbols/test_ids/authoritative doc 均存在）
- standards_violations: 581 源码文件扫描，forbidden violations 7（CODE 3 + COMMENT 4），均为 V19R 注释残留与中文叙述性注释
  - CODE_STANDARD 3: `lib/healpix_db/healpix_drizzle/tests/representative_probe.cpp:2`, `test_spherical_overlap.cpp:1025`, `lib/phase2/tests/synthetic_gate.cpp:5322` 含 `V19R\d`
  - COMMENT_STANDARD 4: 同上 3 + `lib/orchestrator/cpp/src/checkpoint.cpp:406` 含“遍历数组”
- P1 缺陷声明: `tools/file_audit` 缺失（file_audit_before.json 中 defect 字段已声明）

## 产出清单
- `reports/v19r7_quality/file_audit_before.json` — 全量 873 文件清单 + 按模块/扩展名分组 + docs 计数
- `reports/v19r7_quality/standards_violations.json` — 13 标准关键词扫描 + forbidden 明细（文件/行/snippet）
- `reports/v19r7_quality/audit_stats.json` — 汇总：文件数/违规数/broken数/gate 判定
- `reports/v19r7_quality/traceability_broken.json` — TRACEABILITY broken 详情（0 broken）

## 方法
- 替代方案：`find lib -type f | wc -l` 统计 + Python 扫描 TRACEABILITY.csv 的 implementation_files/test_files 存在性 + grep 13 标准关键词（简单关键词匹配）

## 关联
- MASTER_TASK_REGISTER: QA-V19R7-A1-02 TODO→DONE
- Commit: `chore(qa): fix machine_consistency linux path + before scan [QA-V19R7-A1-01/02]`
