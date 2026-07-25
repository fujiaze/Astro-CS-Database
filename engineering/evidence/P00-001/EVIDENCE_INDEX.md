# EVIDENCE_INDEX

| Evidence | Description | SHA-256 |
|---|---|---|
| baseline_inventory.json | 结构化清单：git 信息、工具链版本、13 模块列表、16 个 build 产物 SHA-256、Gaia DR3 SP 20 文件、testdata 3 套 master 校准帧、HISS/HCSD 输出哈希、旧证据来源、范围合规性 | 173DCD83A92D8D46FA47267C8918D5AD85688B5EAA0BDFB2D4833F397895EBA9 |
| TASK_REPORT.md | v1.1 任务报告：Task ID/Commit/Objective/Changes/Files/Compatibility/Rollback/Remaining risks | 8EDBE3FF7600622A2108519C66E2198E299585A7FCCD38DEE8D8E7EB74B9FD9C |
| TEST_REPORT.md | v1.1 测试报告：13 项验证测试表（git/工具链/产物哈希/Gaia/testdata/HISS/HCSD/模块树/范围合规）+ Real-data metrics + Failures | 5D965E28A1DD52B1DA309783B1A88093ED41BC78E1AF238474D9EFF7DCE38783 |
| REVIEW_REPORT.md | v1.1 独立复核报告：Diff reviewed/Tests rerun/Contract findings/Scientific regression/Risks，VERDICT: PASS | BD1C84BBFD280B837DA4EA3370BBD3DD7EDEC9600FB09A15C756D20071E70D28 |
| EVIDENCE_INDEX.md | 本证据索引（self-referential，自身 SHA-256 不列出以避免自引用循环） | (self) |

## 关键源证据引用（来自 baseline_inventory.json，已采集 SHA-256）

### build/artifacts 16 个产物（本任务实际运行 Get-FileHash 采集）

| File | Size | SHA-256 |
|---|---:|---|
| astro_calibration.dll | 997878 | 5C18B1E8A14068851B51762D8AAEBAB7619E01C58A7ECF50147ABC65DBD2B3F5 |
| astro_image_io.dll | 2993875 | 565FABF338A8B20965815A194BA8198C15C9B277AF63B29628203C2B1A994A8A |
| dynamic_psf.dll | 331997 | 51AFFB4AD05737B3146274F8FDDB95208327DB336F4A645FF68C4D0032B43BA2 |
| gaia_client.dll | 281990 | 83CD1267E4C2A488238D9EE0F6B8E012D77377D0976A55522C495EEFAC383338 |
| healpix_browser_qt.exe | 1528326 | DC5E8C2CA1FD0EA099E31EE48D4904689B7A860E7264EC7588F565AF75206274 |
| healpix_drizzle.dll | 1273688 | 54DE6D78AEE963E87B4BA20D8914AB652A3E6D11A47AFC23B0BF1AA9D5A57924 |
| healpix_stack.dll | 1471655 | F99A42D5B62897D307378035D03C57612BC42BB5880E6B638874444E793C09D3 |
| ipv_solver.dll | 886618 | 9860E6985561822E4BF8AD26C4E4DA6E906879D58292578EBFB5F4BB20CD2A7E |
| orchestrator.exe | 3927610 | 04704F1B8687905E8EBF1175482137C419229CB0456426A677BF820F69021385 |
| photometric_calib.dll | 1081805 | E6F9842BAE052D3A8F7DB38A5612F13EBB8A452C99E5467313626DED4CAF7610 |
| snr_estimator.dll | 972836 | F4D72EAB2F1CAF0A3AD67F1667D839001E04115B549574B2434D82797CF7108B |
| star_detector.dll | 1055760 | 2A9F4A027999E95CA1E842634FA8C44D30181B0155D50F3E0DAA24A28C459937 |
| test_browser_backend.exe | 749685 | 923FBC4268DA4514C843B22FC156D487443D3743E22B31F23972085854D86DEE |
| test_healpix_math.exe | 480495 | F945536767C96E3AE4C7E72CA08A14160C69485D80250769EA73590E6BE89D4C |
| test_stf_engine.exe | 388274 | 058220D91521D2FD42B538D99FE39448B9A360C5955591A289501CD64D5A852E |
| a.exe | 127872 | D667664DDB718D73742A86D1A4BBFE566FFAC6D6001584405C7CE18910FFF2FF |

### 已有 pipeline 输出（HISS/HCSD）

| File | Size | SHA-256 |
|---|---:|---|
| lib/orchestrator/cpp/output_hiss_dir/frame1.hiss | 184878332 | 3C06E240D22719CE8CD4FDEAEE0A37127CC06EF72018F012911C5E2FF68C7823 |
| lib/orchestrator/cpp/output_hiss_dir/frame2.hiss | 184886999 | BC2C19FFC17B99F812E7C24028B84FB4856EB0F34F0BC6919FB5EED5233DDAB9 |
| lib/orchestrator/cpp/output_stage2.hcsd | 187455430 | 2A9BD12E0F91BB59ABB170B2765A4806EC5C45FB16045F7C50E131AFA4122C37 |

## 旧证据来源（v1.0，已整合到本任务交付物）

| Source | Description | 引用方式 |
|---|---|---|
| engineering/evidence/P00-001/preflight.json | v1.0 旧基线预检（148KB，模块跟踪文件统计、CI 来源、嵌套 .git 检测） | baseline_inventory.json modules 字段整合 |
| engineering/evidence/P00-001/preflight.md | v1.0 旧基线预检摘要 | TASK_REPORT.md Changes 字段引用 |
| engineering/evidence/P00-001/artifacts.sha256 | v1.0 旧产物哈希（162 字节，仅 16 文件名+哈希） | baseline_inventory.json build_artifacts 字段重新采集覆盖 |
| engineering/evidence/P00-005/environment_baseline.json | v1.0 环境基线（16 工具链版本/路径/许可证/哈希） | baseline_inventory.json toolchain 字段整合 |
| engineering/evidence/P00-008/baseline_manifest.json | v1.0 G0 baseline tag manifest（任务摘要+控制文件哈希+ADR 摘要） | baseline_inventory.json git.tags 与 evidence_legacy_sources 字段引用 |
| engineering_archive_v1.0/evidence/P00-001/ | v1.0 旧证据归档副本 | 回滚备份来源 |
