# Gate 逐项 Checklist

Agent 在每个 Gate 结束时复制对应段到 `evidence/refactor/gates/Gx/CHECKLIST.md`，逐项填 `PASS/FAIL + evidence path + SHA`。空框不能通过。Checklist 完成后自动进入下一 Gate，不等待人工回复。

## G0 起点

- [ ] 控制包 hash 与 `CONTROL_PASS`
- [ ] HEAD/main/origin/main 相同
- [ ] 外部修改全部登记
- [ ] build target/link/entry inventory 现场生成
- [ ] scheduler/thread/global lock inventory 现场生成
- [ ] P0-001..007 有最小证据和后续 Task
- [ ] 未修改科学/生产代码

## G1 合同

- [ ] VERSION 唯一且目标为 `0.10.0-alpha.1`
- [ ] SCI/ALG/DATA/ARCH/API/MOD/TEST ID 唯一
- [ ] Phase1/2 SCI 与冻结约束一致
- [ ] Phase3 状态是 prototype，正式 SCI 已设计
- [ ] 所有端口有 DATA 单位/坐标/invalid/ownership
- [ ] 接缝加性模型与 integration weight 分离
- [ ] 测试容差事前冻结
- [ ] 无未登记科学冲突

## G2 Runtime

- [ ] 显式 CMake targets；无 production GLOB
- [ ] ACR 默认 OFF
- [ ] Result/error/cancel 单测
- [ ] DataArtifact/provenance roundtrip
- [ ] Registry duplicate/ABI/contract negative tests
- [ ] Pipeline cycle/port/unit negative tests
- [ ] RunContext 无 singleton/global scheduler
- [ ] 2 核 DAG 并发、backpressure、cancel、recovery
- [ ] JSONL trace schema/sequence

## G3 I/O 与 CPU

- [ ] `aio_free_image_data` canonical RAII；LSan零泄漏
- [ ] `fits_is_reentrant()==1`
- [ ] worker-local FITS handles；TSan/ASan压力通过
- [ ] Phase2全局读锁移除
- [ ] baseline/AVX2/AVX512 provider ABI/correctness
- [ ] CPUID+OS state+quota 探测
- [ ] benchmark/profile/fallback/invalid-profile tests
- [ ] 每 heavy node 自动 resource monitor

## G4 Phase1

- [ ] old symbols 全映射，无能力遗失
- [ ] calibration/cosmetic Oracle
- [ ] Star/PSF truth catalog tests
- [ ] PlateSolve WCS roundtrip
- [ ] Photometry integration regression
- [ ] Noise/SNR analytic + Monte Carlo
- [ ] Drizzle constant/flux/centroid/support/wrap
- [ ] HiPS writer/verify
- [ ] canonical IR = runtime trace
- [ ] 2 核 heavy资源门；无泄漏

## G5 Phase2

- [ ] sampler 无固定1 worker
- [ ] coverage/control deterministic
- [ ] UPM additive/gauge/disconnected graph defined
- [ ] seam synthetic 改善且源 flux 保持
- [ ] block plan内存有界；无稠密巨缓存
- [ ] rejection各方法及auto reason
- [ ] integration weight/variance/support单位清楚
- [ ] diagnostics HiPS artifacts完整
- [ ] canonical IR = runtime trace
- [ ] 无低利用/全局锁/黑洞/条纹

## G6 Phase3

- [ ] prototype退出生产注册
- [ ] SCI/ALG/DATA先于实现
- [ ] BUNIT/provenance/output path无硬编码
- [ ] WCS plan/overflow/尺寸校验
- [ ] tile并行；生产无二维串行主循环
- [ ] constant/analytic/impulse/wrap/pole/coverage tests
- [ ] FITS reopen/header/WCS/units verify
- [ ] 资源门通过后才 IMPLEMENTED

## G7 唯一生产路径

- [ ] CLI不include科学内部实现
- [ ] phase all真正传 Artifact ID/hash
- [ ] test/benchmark/doctor无stub
- [ ] direct drizzle退出
- [ ] old Orchestrator退出
- [ ] AIO PipelineEngine退出调度
- [ ] old Stage2退出
- [ ] production link/module list无ACR

## G8 文档与质量

- [ ] L0六文档完整且简洁
- [ ] 每production module有L2 README
- [ ] Contract graph PASS
- [ ] AST/API zero drift
- [ ] Registry/CMake/link/docs zero drift
- [ ] static graph/runtime trace zero drift
- [ ] serial/hardcode checker zero violation
- [ ] compiler warning/blanket suppression清零
- [ ] ASan/UBSan/LSan/TSan无未解决错误
- [ ] duplicate scheduler/I/O/WCS清理闭环
- [ ] SBOM/reproducible metadata

## G9 Linux

- [ ] GCC Release clean build
- [ ] Clang Debug/static analysis
- [ ] 全模块 synthetic TEST
- [ ] 2核 resource/cancel/recovery
- [ ] 少量真实 hash/smoke
- [ ] 所有外部命令 timeout/log

## G10 Windows

- [ ] Fatduck identity/data/toolchain
- [ ] main commit与Linux一致
- [ ] MSVC clean Debug/Release
- [ ] CPU benchmark/profile/provider
- [ ] Windows synthetic/resource
- [ ] 三块代表帧
- [ ] final candidate冻结
- [ ] 32R 11+11+10 全部贡献
- [ ] Phase2 mosaic/support/UPM/rejection
- [ ] Phase3 FITS verify
- [ ] locked HiPS views和seam metrics

## G11 发布

- [ ] Linux/Windows Alpha包只含一个用户入口
- [ ] baseline provider随包；高级provider安全fallback
- [ ] VERSION/commit/build id/SBOM/checksums一致
- [ ] L0-L3最终真相校验
- [ ] P0/P1清零；无NOT VERIFIED（除owner review）
- [ ] 审核包白名单/大小/敏感/SHA/解包复验
- [ ] `validate_audit.py` PASS
- [ ] 状态仅 READY_FOR_OWNER_REVIEW
- [ ] 负责人HiPS视觉审核与最终决定
