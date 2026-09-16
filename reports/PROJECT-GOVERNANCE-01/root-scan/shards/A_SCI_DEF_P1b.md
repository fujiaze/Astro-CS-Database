# A_SCI_DEF_P1b 分片报告（ROOT-004 旧 bug 清单按最新权威订正）

- 分片名：A_SCI_DEF_P1b（原类别 A_SCI_DEF，原优先级 P1）
- 产物：`reports/PROJECT-GOVERNANCE-01/root-scan/shards/A_SCI_DEF_P1b.psv`（39 行 = 表头 1 + finding 38；列数恒为 10）
- 回话基线：任务书 ecf6ad6f；开工实测 HEAD=main=2c328348；收尾实测 939d3f6c（并行治理线提交，本分片零 git 写）
- 四态计数：OPEN 38 ｜ RESOLVED 0 ｜ VOID 0 ｜ UNVERIFIABLE 0
- ID 覆盖自证（命令 + 逐字输出）：
  `python3 -c "L=open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/A_SCI_DEF_P1b.psv',encoding='utf-8').read().splitlines();A=open('reports/PROJECT-GOVERNANCE-01/root-scan/shards/_assign/A_SCI_DEF_P1b.tsv',encoding='utf-8').read().splitlines();ids=[x.split('|')[0] for x in L[1:]];aids=[x.split(chr(9))[0] for x in A[1:] if x.strip()];print(len(L),sorted(set(len(x.split('|')) for x in L)),len(aids),len(ids),ids==aids,[i for i in aids if i not in ids])"`
  → 输出：`39 [10] 38 38 True []`
- 归属分布：P1-001 11 ｜ P1-002 6 ｜ P2-001 4 ｜ DOC-001 4 ｜ DATA-001 3 ｜ P2-002 3 ｜ P3-002 2 ｜ P3-001/ARCH-001/OBS-001/MOD-001/AIO-001 各 1
- GAP 关系：仅 V12-N-08 记「与 GAP-015 重复」（资源门双实现）；其余 37 条记「无」
- UNVERIFIABLE 清单：无（0 条）——每条均在本轮以 read/grep/sed/python 在当前树复现，未以 finding 自述为据
- 最重要的 3 条 OPEN（判词）：
  1. `W2-N-08`：真帧 testdata/NGC55_T3_flying_dutchman 复算 `abs(CD1_1)·3600=0.009856″` 对 `sqrt(abs(det))·3600=0.958577″` 偏差 −98.97%，该值即 PHASE2_SAMPLER §5.4 K_CORR_DOMAIN 的输入 ⇒ 下游恒被 clamp 到 300″ 档。
  2. `M7-A-135`：实现已无任何跳步（sdet_api.cpp:1854-1855 无 stride，:1985 注释自述旧 `x += xr;` 游标回写已按缺陷删除），而文档 §3:93 与 §6:148 仍互斥描述 `x+=5`，判据锚 :1715-1734 悬空。
  3. `W2-N-04`：orchestrator.cpp:3007-3008 仍无条件写 `PHOTSCAL=out_scale` 与 `PHOTAPPL="1"`，不 gate `out_n_matched>0`/scale≠1，退化恒等拷贝帧被声明为已应用测光。
- 异常（影响复算与后续合并）：
  1. HEAD 漂移（见上）；行号一律按各自读取时点重定位，finding 行号漂移本身已作为 E_TRACE_BREAK 证据写入 col6。
  2. 判据路径漂移（finding 所引 → 当前树实为）：`docs/science/PHASE2_SAMPLER.md`→`docs/algorithms/PHASE2_SAMPLER.md`；`docs/science/PHOTOMETRIC_FIT.md`→`docs/algorithms/PHOTOMETRIC_FIT.md`；`lib/star_detection/sdet_api.cpp`→`lib/star_detector/src/sdet_api.cpp`；`lib/star_detector/src/star_detector.cpp`→`lib/phase1/stars/star_detector.cpp`；`lib/phase1/photometry/image_corrector.cpp`→`lib/photometric_calib/cpp/src/image_corrector.cpp`。
  3. M9-A-1 行号漂移：drizzle_engine.cpp 的 cos_thresh_60 243→244、THRESH_60ARCSEC 1652→1770、use_adaptive 1362→1385。
  4. M7-A-126 文档/代码互斥：REJECTION_ALGORITHMS.md §12 的行锚把 linear_fit 指向 rejection.cpp:1407，而实现为 :1386-1458（σ=Σ abs(residual)/N）。
  5. W2-N-02 同名异类型：`has_gain` 只存在于 lib/calibration/include/astrocs/calibration/v6_calibration_covariance.h:83（另一类型），AIOCalibrationMetadata 仍无该判别位。
  6. M7-A-121 半闭合：flags→因子映射已在 PHASE2_UPM_IMPL.md:163 + upm.cpp:181-185 定义（该子事实已闭），故 col10 写明「仍开的是同名两类型互不指认 + quality_mode 与 (void)mode 不符」，未按旧判据口径整条套用。
  7. 无法定位的 ID：无；col5 引用的每个节号/条款均已 read/grep 核对在位（无编造节号）。
- 命令日志：`run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/A_SCI_DEF_P1b.log`
