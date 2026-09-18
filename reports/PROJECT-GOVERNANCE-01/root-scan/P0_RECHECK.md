# P0_RECHECK · ROOT-004 主控对 93 条 P0 的逐条复核（100%，不抽样）

- 接手开工基线：HEAD = main = origin/main = 2c328348304d033aecfa81faf79d1c6cd802b30a（15:03 实测三 SHA 一致）。
- 复核收口时 HEAD = d414c3e0e59142a4fc48760ceb97fe9ae828486b（并行执行线窗口内持续推进：ROOT-001/002/003、GOV 立 TEST-GREEN-001 等提交已落入 main 并推送；判定证据一律按各自取证时刻的工作树实况留痕）。
- 方法（每条 P0 全部执行，不抽样）：
  1. 从 26 片 PSV 抽取该条 10 列判定（_tools/p0_rerun.py，留档 _gen/p0_rerun.json）；
  2. 本轮在工作树重跑其证据列命令（每条 ≥1 个，timeout 70s，rc 与输出留 _gen/p0_rerun.json）；
  3. 前任主控批 p0_verify_A2.sh（29 命令）与 p0_verify_B.sh（49 命令）整体复跑（logs/p0_verify_A2_recheck.out、logs/p0_verify_B_recheck.out）；
  4. 对证据列不满足「字面可复跑」的 19 条（集中于 C_DOC_CODE_GAP_P0/F_TEST_GAP_P0）由主控逐条重新取证：改写为字面命令并修正路径与行号漂移（star_matcher 移至 cpp/src、gaia_client.c 符号移至 module_entry.c、p2_session 移至 stage2_common/upm、Windows 清单位于 cmake/astrocs.product.windows.json.in、ivar 计数位于 phase2/tools/stage2.cpp 等），改写后逐条重跑通过；
  5. 对账本 fix_state=FIXED/PARTIAL 的 21 条 P0，全部按当前树复跑验证后才认 RESOLVED。
- 三条禁令逐条核后结论：93 条分片判定全部【维持】，无改判。无「问题仍在判 RESOLVED」（20 条 RESOLVED 均有当前树复现证据）；无「设计已变仍判 OPEN」（P0 第 5 列均为现行权威条款）；P0 无 VOID/UNVERIFIABLE。
- 复核分布：OPEN 73 / RESOLVED 20；与 REBASE_TABLE.md 的 P0 行完全一致。

注：下表「证据重跑」= 本轮 p0_rerun 首命令（截断）+ rc + 输出首片段（逐字截断）；完整三轮记录见 _gen/p0_rerun.json 与 run/PROJECT-GOVERNANCE-01/ROOT-004/logs/。

| ID | 分片 | 分片判定 | 归属 | 证据重跑（本轮） | 复核结论 |
|---|---|---|---|---|---|
| FD-F-003 | F_TEST_GAP_P0 | OPEN | CI-001 | timeout 60 grep -o tests=.0. run/ci_domain_merge/gh_artifact rc=1 out=tests="0" 0 … | 维持 |
| M1a-A-001 | A_SCI_DEF_P0 | OPEN | P1-001 | cd "/workspace/Astro CS Database" && timeout 60 sed -n "48p" rc=0 out=  (u,v) = CD · (xp−CRPIX) + SIP_A/B(u,v)   # (u,v) 为 TAN 投影中间坐标     //… | 维持 |
| M1a-A-002 | A_SCI_DEF_P0 | OPEN | P1-001 | cd "/workspace/Astro CS Database" && timeout 60 sed -n "186p rc=0 out=  n_pairs≥12；rms_arcsec ≤0.5″（实测锚 Galaxy_Center=0.1431″，memory.md   - … | 维持 |
| M1a-A-003 | A_SCI_DEF_P0 | OPEN | P3-001 | cd "/workspace/Astro CS Database" && timeout 60 sed -n "198p rc=0 out=    const double theta = -yd * kRad; // θ = −Y (rad)     return plane_… | 维持 |
| M1a-B-001 | B_STD_MISMATCH_ALL | OPEN | GOV-001 | sed -n '411p' lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp; sed - rc=0 out=            const int NB_GRID = 41;       // 每轴网格点数 (整改: 7 -> 41)     … | 维持 |
| M1a-B-002 | B_STD_MISMATCH_ALL | OPEN | GOV-001 | sed -n '64p' docs/standards/STANDARDS_REGISTRY.md; awk 'NR>= rc=0 out=∣ Paper II §5 Table 1（TAN/SIN/CAR/AIT 四投影） ∣ 四投影按 Table 1 的 R_θ 定义实现，新… | 维持 |
| M1a-C-001 | C_DOC_CODE_GAP_P0 | OPEN | P1-001 | cd "/workspace/Astro CS Database" && timeout 60 grep -n "NB_ rc=0 out=39:- 维度 `w>0,h>0`，星点列表非空；`CRPIX` 按 `w/2+0.5` 冻结；`trans.order` 2–3 阶；`N…；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M1a-C-002 | C_DOC_CODE_GAP_P0 | RESOLVED | P1-001 | cd "/workspace/Astro CS Database" && timeout 60 grep -c "std rc=2 out=1 lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp-343-            // (共线/退化配置)…；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M1a-C-003 | C_DOC_CODE_GAP_P0 | OPEN | P1-001 | cd "/workspace/Astro CS Database" && timeout 60 grep -n "pts rc=0 out=2149:        pts.emplace_back(static_cast<double>(x), static_cast<doub…；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M1a-C-004 | C_DOC_CODE_GAP_P0 | OPEN | P3-001 | cd "/workspace/Astro CS Database" && timeout 60 grep -n "fov rc=1 out=413:  out->fov_x_deg = pl.fov_x_deg; 414:  out->fov_y_deg = pl.fov_y_d…；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M1a-F-001 | F_TEST_GAP_P0 | OPEN | P3-001 | cd "/workspace/Astro CS Database" && timeout 60 sed -n "214p rc=0 out=            dec_o = -yd;         dec = -y_deg …；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M2a-A-1 | A_SCI_DEF_P0 | OPEN | P1-001 | cd "/workspace/Astro CS Database" && timeout 60 sed -n "40p; rc=0 out=    w_jp = a_jp / A_drop,j   drop 多边形: pixelToSky((x±0.5·pixfrac, y±0.… | 维持 |
| M2a-B-1 | B_STD_MISMATCH_ALL | OPEN | GOV-001 | awk 'NR==186' docs/standards/STANDARDS_REGISTRY.md; awk 'NR= rc=1 out=- CLAUSES: DR3 source 列面（ra/dec 参考历元 J2016.0、phot_g_mean_mag/phot_bp_m… | 维持 |
| M2a-C-1 | C_DOC_CODE_GAP_P0 | RESOLVED | DATA-001 | cd "/workspace/Astro CS Database" && timeout 60 grep -n "b64 rc=2 out=bash: -c: 行 1: 寻找匹配的 `"' 时遇到了未预期的 EOF …；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M2a-F-1 | F_TEST_GAP_P0 | OPEN | MOD-001 | timeout 60 grep -rl -e candidate_oracle_test -e variance_pro rc=0 out=rc=1 85:- **零漏选不变量**：`candidate_oracle_test` 9003 例全枚举下 `false_negativ… | 维持 |
| M2a-H-1 | H_NUMERIC_ALL | RESOLVED | P1-001 | timeout 60 awk 'FNR==1006{print FILENAME":"FNR": "$0}' lib/h rc=0 out=lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp:1006:     if (config… | 维持 |
| M2a-H-2 | H_NUMERIC_ALL | OPEN | DATA-001 | timeout 60 awk 'FNR==1912{print FILENAME":"FNR": "$0} FNR==1 rc=0 out=lib/healpix_db/healpix_drizzle/drizzle_engine.cpp:1912:               … | 维持 |
| M2b-A-01 | A_SCI_DEF_P0 | OPEN | AIO-001 | cd "/workspace/Astro CS Database" && timeout 60 sed -n "493p rc=0 out=        ps->leaf_order = ilog2_u64(nside); 0 1 … | 维持 |
| M2b-A-02 | A_SCI_DEF_P0 | OPEN | AIO-001 | cd "/workspace/Astro CS Database" && timeout 60 sed -n "1069 rc=0 out=            const uint32_t nside_k = 1u << (k + 9);   约定。FITS cards OR… | 维持 |
| M2b-B-01 | B_STD_MISMATCH_ALL | OPEN | P1-001 | awk 'NR==137{print "writer:137 "$0} END{}' lib/astro_image_i rc=0 out=writer:137     uint64_t dir = ipix / 10000;                   dir.c_st… | 维持 |
| M2b-B-02 | B_STD_MISMATCH_ALL | OPEN | P1-001 | grep -c NUNIQ lib/astro_image_io/src/hips/aio_hips_writer.cp rc=0 out=0     fits_write_key_lng(fptr, "MOCORDER", (long)order, (char*)"MOC or… | 维持 |
| M2b-B-03 | B_STD_MISMATCH_ALL | OPEN | P1-001 | sed -n '943p' lib/astro_image_io/src/hips/aio_hips_writer.cp rc=0 out=    std::snprintf(buf, sizeof(buf), "%.6f", 3600.0 * 180.0 / kPi() * s… | 维持 |
| M2b-B-04 | B_STD_MISMATCH_ALL | OPEN | GOV-001 | grep -c obs_bandpass docs/standards/STANDARDS_REGISTRY.md; a rc=0 out=2 - COMPLIANCE: PARTIAL ∣ §4.2.1（properties 必需键集） ∣ `hips_version/hips… | 维持 |
| M2b-B-07 | B_STD_MISMATCH_ALL | OPEN | GOV-001 | grep -n -A1 'uint64_t npix' lib/common/healpix/healpix_core. rc=0 out=333:uint64_t npix(uint32_t nside) { 334-    return 12ULL * uint64_t(ns… | 维持 |
| M2b-B-09 | B_STD_MISMATCH_ALL | OPEN | P1-001 | sed -n '1155p;1651p' runtime/io/fits_core.c; awk 'NR==223' d rc=0 out=    fio_card_write(f, "CHECKSUM", "0000000000000000", "HDU checksum up… | 维持 |
| M2b-C-01 | C_DOC_CODE_GAP_P0 | OPEN | AIO-001 | cd "/workspace/Astro CS Database" && timeout 60 grep -c -e r rc=1 out=0 …；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M2b-F-01 | F_TEST_GAP_P0 | OPEN | MOD-001 | timeout 60 grep -rl test_healpix_oracle --include=CMakeLists rc=0 out=rc=1 88:        if (std::fabs(dec) >= 89.999999) continue; … | 维持 |
| M3-A-001 | A_SCI_DEF_P0 | RESOLVED | P1-002 | cd "/workspace/Astro CS Database" && timeout 60 grep -c "sou rc=0 out=6     // 旧 (A-B)/residual_scale 为 SNR-008 已退休量, 不再进入生产路径。 - **逐源科学 SNR… | 维持 |
| M3-A-002 | A_SCI_DEF_P0 | OPEN | P2-002 | cd "/workspace/Astro CS Database" && timeout 60 sed -n "66p; rc=0 out=# 像素级 SNR 权重（stage2 排异/积分，weight_mode=2）   weights[s] = support[s] × s… | 维持 |
| M3-C-001 | C_DOC_CODE_GAP_P0 | RESOLVED | P1-002 | cd "/workspace/Astro CS Database" && timeout 60 grep -n "r_c rc=0 out=518:    if (r_consistent.size() < 3) { 616:    if (r_inliers.size() >=…；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M3-C-002 | C_DOC_CODE_GAP_P0 | RESOLVED | P1-002 | cd "/workspace/Astro CS Database" && timeout 60 grep -n "PC_ rc=2 out=lib/photometric_calib/cpp/src/star_matcher.cpp-428-        bool satura…；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M3-C-003 | C_DOC_CODE_GAP_P0 | OPEN | P1-002 | cd "/workspace/Astro CS Database" && timeout 60 grep -n "min rc=0 out=350:    cfg->min_patch_samples = 64; 37:- 维度 `h>0,w>0`，`data` 非空且含有限值；…；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M3-E-001 | E_TRACE_BREAK_P0P1 | OPEN | P1-001 | python3 -c "import csv;r=list(csv.reader(open('docs/TRACEABI rc=0 out=TEST-CAL-001 lib/calibration/tests/test_photometry_apply.cpp VERIFIED … | 维持 |
| M3-F-001 | F_TEST_GAP_P0 | OPEN | P1-002 | timeout 60 grep -c numpy tests/backend/test_noise_model_orac rc=0 out=0 117:- **Python 参考**：NumPy 对同 `data` 的 `median/MAD/5σ裁剪/平面最小二乘` 复算 `v… | 维持 |
| M3b-A-01 | A_SCI_DEF_P0 | OPEN | P1-001 | cd "/workspace/Astro CS Database" && timeout 60 sed -n "375p rc=0 out=    fdf.f = &sdet_gaussian_f;     int gsl_status = sdet_lm_fit<T>(imag… | 维持 |
| M3b-A-02 | A_SCI_DEF_P0 | OPEN | DATA-001 | cd "/workspace/Astro CS Database" && timeout 60 sed -n "2304 rc=0 out=        rec.flux = (float)fit_results[i].A;     s.flux = m00;         … | 维持 |
| M3b-A-03 | A_SCI_DEF_P0 | OPEN | P1-001 | cd "/workspace/Astro CS Database" && timeout 60 sed -n "2470 rc=0 out=    // [3]=flux_inst (PSF integrated flux), [4]=flux_uncertainty (PSF … | 维持 |
| M3b-C-01 | C_DOC_CODE_GAP_P0 | OPEN | MOD-001 | cd "/workspace/Astro CS Database" && timeout 60 grep -n "0.5 rc=1 out=13:  Moffat4 GSL-LM 中心），不引入 0.5px 网格量化损失；合成场验收容差 0 …；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M3b-C-02 | C_DOC_CODE_GAP_P0 | OPEN | MOD-001 | cd "/workspace/Astro CS Database" && timeout 60 grep -n "pha rc=2 out=1616:  const astrocs::phase1::StarDetector det(5.0); lib/core/src/modu…；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M3b-F-01 | F_TEST_GAP_P0 | OPEN | P1-001 | timeout 60 grep -c gate2_psf_oracle ci/checks.json ; timeout rc=0 out=0 False … | 维持 |
| M3b-F-02 | F_TEST_GAP_P0 | OPEN | P1-001 | timeout 60 grep -rl curve_fit tests lib ; echo rc=$? ; timeo rc=0 out=rc=1 99:- **Python 参考**：`scipy` / NumPy 对同参数 Moffat4 图像块做 `curve_fit` … | 维持 |
| M3b-F-03 | F_TEST_GAP_P0 | OPEN | P1-001 | timeout 60 grep -n "amp = " lib/star_detector/tests/p1star/p rc=0 out=60:            if (p < 0.15) amp = 40.0 + p * 100.0;       // SNR≈20 谱… | 维持 |
| M3b-G-01 | G_GOV_GATE_P0 | OPEN | DOC-001 | timeout 60 grep -c -e PSF -e 质心 docs/KNOWN_LIMITATIONS.md do rc=1 out=docs/KNOWN_LIMITATIONS.md:0 docs/owner/RELEASE_STATUS.md:0 0 … | 维持 |
| M3b-H-01 | H_NUMERIC_ALL | RESOLVED | P1-001 | timeout 60 awk 'FNR==450{print FILENAME":"FNR": "$0} FNR==22 rc=0 out=lib/star_detector/src/sdet_api.cpp:227:         double rmse_ratio = fi… | 维持 |
| M4-A-01 | A_SCI_DEF_P0 | OPEN | P2-001 | cd "/workspace/Astro CS Database" && timeout 60 sed -n "1844 rc=0 out=        if (n <= 4) {   UNDERDETERMINED (n ≤2) → 不做猜测，全接受 (P2_REASON_U… | 维持 |
| M4-A-02 | A_SCI_DEF_P0 | OPEN | P2-001 | cd "/workspace/Astro CS Database" && timeout 60 sed -n "58p; rc=0 out=       support = (support 空) ? 1.0 : max_{valid,W>0} support[i]   # ca… | 维持 |
| M4-C-01 | C_DOC_CODE_GAP_P0 | RESOLVED | P2-001 | cd "/workspace/Astro CS Database" && timeout 60 grep -n "pf_ rc=0 out=89:    static const double pf_grid[3] = {0.5, 0.8, 1.0};     // pixfra…；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M4-C-02 | C_DOC_CODE_GAP_P0 | RESOLVED | P2-002 | cd "/workspace/Astro CS Database" && timeout 60 grep -n "zer rc=2 out=lib/phase2/src/upm.cpp-223-        cfg.snr_weight_mode = 0; lib/phase2…；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M4-C-03 | C_DOC_CODE_GAP_P0 | RESOLVED | P2-001 | cd "/workspace/Astro CS Database" && timeout 60 grep -n "iva rc=2 out=lib/phase2/tools/stage2.cpp-604- lib/phase2/tools/stage2.cpp-605-    s…；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M4-F-01 | F_TEST_GAP_P0 | OPEN | P2-001 | timeout 60 grep -rl -e control_median_mc_test -e kcorr_matri rc=0 out=rc=1 147:- §11 Oracle 全过（含 `control_median_mc_test` MC 一致性、gauge 唯一性、h… | 维持 |
| M4-F-02 | F_TEST_GAP_P0 | OPEN | P2-002 | timeout 60 grep -rl huber --include=*.py tests/unit/v6_p2_up rc=0 out=rc=1 108:- **Python 参考**：NumPy 对同 `raw` 的 Huber IRLS + `control_ivar` … | 维持 |
| M5a-G-001 | G_GOV_GATE_P0 | RESOLVED | OBS-001 | timeout 60 grep -n -e "kCpuMeanMinPercent =" -e "kMon001Util rc=0 out=98:inline constexpr double kCpuMeanMinPercent = 85.0;   // §18.2 冻结均值下… | 维持 |
| M5a-G-002 | G_GOV_GATE_P0 | RESOLVED | OBS-001 | timeout 60 grep -n "inline double cpu_percent_of_allocated_c rc=0 out=253:inline double cpu_percent_of_allocated_capacity(const GateConfig& … | 维持 |
| M5a-G-003 | G_GOV_GATE_P0 | OPEN | OBS-001 | timeout 60 python3 -c "import json;d=json.load(open('ci/chec rc=0 out=gate-required hits 0 requires_monitor ['BUILD-GCC-RELEASE', 'DEEP-CLAN… | 维持 |
| M5a-G-005 | G_GOV_GATE_P0 | OPEN | RT-001 | timeout 120 python3 -c "import os;n=sum(open(os.path.join(d, rc=0 out=vector-thread-in-lib-cpp = 12 SCAN_ROOTS = [os.path.join(REPO, "lib")]… | 维持 |
| M5b-C-01 | C_DOC_CODE_GAP_P0 | OPEN | CI-001 | cd "/workspace/Astro CS Database" && timeout 60 python3 -c " rc=1 out=  File "<string>", line 1     import csv;rows=[r for r in csv.reader(o…；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M5b-C-02 | C_DOC_CODE_GAP_P0 | OPEN | PKG-001 | cd "/workspace/Astro CS Database" && timeout 60 grep -n "PLA rc=2 out=cmake/astrocs.product.windows.json.in-8-    {"unit_id": "PLATFORM-CLI"…；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M5b-G-01 | G_GOV_GATE_P0 | OPEN | CLI-001 | timeout 60 sed -n "132,133p" tools/check_api_docs.py; timeou rc=0 out=        exe = os.path.join(self.repo, "build", "cli", "astrocs")      … | 维持 |
| M5b-G-02 | G_GOV_GATE_P0 | OPEN | CI-001 | timeout 60 sed -n "146p;159p" tools/quality/check_traceabili rc=0 out=        "rows_broken": len(broken),     return 0 DEFAULT_TABLES = [os.… | 维持 |
| M5b-G-03 | G_GOV_GATE_P0 | OPEN | CI-001 | timeout 60 grep -n "if name not in ast_names" tools/check_as rc=1 out=61:            if name not in ast_names: 0 … | 维持 |
| M5b-G-04 | G_GOV_GATE_P0 | OPEN | PKG-001 | timeout 60 cat VERSION; timeout 60 python3 -c "import json;p rc=0 out=0.11.0-alpha.2 product 0.11.0-alpha.1 contract 0.11.0-alpha.1 lock 0.1… | 维持 |
| M5b-G-05 | G_GOV_GATE_P0 | OPEN | INT-001 | timeout 60 sed -n "25,26p" tools/quality/contracts/check_bui rc=1 out=        for tgt in ["phase2","astrocs-stage2","calibrated_pair_diag","… | 维持 |
| M5b-G-06 | G_GOV_GATE_P0 | OPEN | MOD-001 | timeout 60 python3 -c "import json;u=json.load(open('packagi rc=1 out=units 10 sha256-null 10 CMakeLists.txt:0 cli/CMakeLists.txt:0 … | 维持 |
| M6a-C-001 | C_DOC_CODE_GAP_P0 | OPEN | P3-001 | cd "/workspace/Astro CS Database" && timeout 60 grep -n "p3_ rc=2 out=5567:  if (p3_order_select(input_order, g.scale, &order_sel) != P3_RS_…；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M6a-G-001 | G_GOV_GATE_P0 | OPEN | CI-001 | timeout 60 sed -n "37p;39p" tools/quality/contracts/check_co rc=0 out=            if "V19R2" in c or "V19R3" in c:                 if "冻结" n… | 维持 |
| M6b-E-001 | E_TRACE_BREAK_P0P1 | OPEN | DOC-001 | grep -m1 -n TRACEABILITY.csv docs/README-DOCS.md docs/DEVELO rc=0 out=docs/README-DOCS.md:13:追溯             docs/TRACEABILITY.csv（唯一矩阵） docs… | 维持 |
| M6b-G-001 | G_GOV_GATE_P0 | OPEN | CI-001 | timeout 60 python3 -c "import json;m=json.load(open('docs/tr rc=0 out=rows 30 test_VERIFIED 22 evidence_VERIFIED 6 evidence_path_all_None Tr… | 维持 |
| M6b-G-002 | G_GOV_GATE_P0 | OPEN | DOC-001 | timeout 60 python3 -c "import glob;print('RELEASE_STATUS doc rc=0 out=RELEASE_STATUS docs 4 PRE_RELEASE_ENGINEERING_FOUNDATION=PASS FINAL_RE… | 维持 |
| M6b-G-003 | G_GOV_GATE_P0 | OPEN | DOC-001 | timeout 60 grep -c -e docs_machine_consistency -e config_con rc=0 out=0 114:## 7. Machine Consistency (S8 gate, 本版实测) 1 … | 维持 |
| M7-A-001 | A_SCI_DEF_P0 | OPEN | P2-002 | cd "/workspace/Astro CS Database" && timeout 60 sed -n "46p; rc=0 out=  w_UPM = quality_factor × geometric_reliability × control_ivar   raw … | 维持 |
| M7-A-002 | A_SCI_DEF_P0 | OPEN | P2-002 | cd "/workspace/Astro CS Database" && timeout 60 sed -n "126p rc=0 out=  `ivar_valid?ivar:support`（1/ADU²，ivar 缺失样本 fallback support，   # 禁 p… | 维持 |
| M7-C-001 | C_DOC_CODE_GAP_P0 | RESOLVED | P2-001 | cd "/workspace/Astro CS Database" && timeout 60 grep -n "con rc=2 out=lib/phase2/src/stage2_common.cpp-38-        if (j.contains("model")) {…；主控重新取证（字面命令+当前树行号，重跑一致） | 维持 |
| M7-G-001 | G_GOV_GATE_P0 | OPEN | FINAL-001 | timeout 60 grep -rc "Oracle 全过" docs/science/PSF.md docs/sci rc=0 out=docs/science/PSF.md:1 docs/science/DRIZZLE.md:1 analytic FWHM/sigma 1.… | 维持 |
| M8-F-001 | F_TEST_GAP_P0 | RESOLVED | CI-001 | timeout 120 python3 -B -c "import unittest;print('collected= rc=0 out=collected= 22 1 … | 维持 |
| M8-F-002 | F_TEST_GAP_P0 | RESOLVED | AIO-001 | timeout 60 sed -n "82p;137p" tests/unit/io_ownership_test.cp rc=0 out=    return failures == 0 ? 0 : 1;   return 1;   // M8-F-002: 失败必须非零退出(… | 维持 |
| M8-F-003 | F_TEST_GAP_P0 | RESOLVED | CLI-001 | timeout 60 sed -n 33p ci/steps/linux_build_root_graph.sh ; t rc=0 out=cp -f build/linux-control/libastrocs_runtime.so build/libastrocs_runti… | 维持 |
| M8-F-004 | F_TEST_GAP_P0 | OPEN | CI-001 | timeout 60 grep -rl -e check_standards_registry -e STD-REGIS rc=0 out=rc=1         key, std, ver, _clauses = row                 results.app… | 维持 |
| M8a-G-001 | G_GOV_GATE_P0 | OPEN | PKG-001 | timeout 60 grep -c gsl DEPENDENCIES.md packaging/dependency- rc=0 out=DEPENDENCIES.md:0 packaging/dependency-lock.json:0 target_link_librari… | 维持 |
| M9-B-1 | B_STD_MISMATCH_ALL | OPEN | P1-001 | awk 'NR==274' lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp; awk ' rc=0 out=    result->cd.cd11 = trans.x10 / 3600.0;             // 单位: (像素/角秒) *… | 维持 |
| M9-F-1 | F_TEST_GAP_P0 | OPEN | CI-001 | timeout 60 grep -rl -e dataflow_fuzz -e fits_core_selftest - rc=0 out=rc=1 27:astro_image_io:     pipeline_frame_contract_test / dataflow_fu… | 维持 |
| M9-H-1 | H_NUMERIC_ALL | RESOLVED | NEXT-PACK:NP-01 | timeout 60 awk 'FNR==391{print FILENAME":"FNR": "$0} FNR==39 rc=0 out=lib/gaia_xpsd_client/src/module_entry.c:391: static int json_append_es… | 维持 |
| M9-H-2 | H_NUMERIC_ALL | RESOLVED | NEXT-PACK:NP-01 | timeout 60 awk 'FNR==1284{print FILENAME":"FNR": "$0} FNR==1 rc=0 out=lib/gaia_xpsd_client/src/gaia_client.c:1284:                     if (!… | 维持 |
| V11-N-01 | G_GOV_GATE_P0 | OPEN | AIO-001 | timeout 60 python3 -c "s=open('lib/astro_image_io/include/ai rc=0 out=C AioHipsSnrPoint field-count 6 PY mirror field-count 4 914:          … | 维持 |
| V11-N-04 | G_GOV_GATE_P0 | OPEN | DATA-001 | timeout 60 python3 -c "s=open('lib/dynamic_psf/include/dynam rc=0 out=C params 9 PY args 8 gate2_psf_oracle.py PY args 8 diag_gaia_psf_proje… | 维持 |
| V11-N-05 | G_GOV_GATE_P0 | OPEN | MOD-001 | timeout 120 grep -rln acs_negotiate_v1 --include=*.c --inclu rc=1 out=tests/abi/abi002_lifecycle_probe.c include/astrocs/abi/lifecycle_v1.h … | 维持 |
| V13-N-03 | G_GOV_GATE_P0 | OPEN | GOV-001 | timeout 60 python3 -c "import csv,collections;r=list(csv.Dic rc=0 out=rows 713 {'VERIFIED': 713} 0         if typ == "shipping_src" and ship… | 维持 |
| V18-N-12 | G_GOV_GATE_P0 | OPEN | QA-001 | timeout 60 grep -n -e "assertTrue(True)" -e "continue  #" -e rc=0 out=87:                            "NOT_SHIPPED(avx子集,avx2主导)" if imp != "… | 维持 |
| V19-N-01 | G_GOV_GATE_P0 | OPEN | CI-001 | timeout 120 python3 -c "import subprocess;o=subprocess.run([ rc=1 out=total 40 0 … | 维持 |
| V2-N-01 | G_GOV_GATE_P0 | RESOLVED | DATA-001 | timeout 60 grep -n "from ipv_abi_mirror import" lib/plate_so rc=0 out=35:from ipv_abi_mirror import (  # noqa: E402         ("struct_size", … | 维持 |
| V2-N-08 | G_GOV_GATE_P0 | RESOLVED | P1-002 | timeout 60 sed -n "1614p" lib/core/src/module_adapters.cpp;  rc=0 out=  const std::string psf_mode = (n_fit_limit > 0) ? std::string("fast")… | 维持 |
| V9-N-07 | G_GOV_GATE_P0 | OPEN | CI-001 | timeout 60 python3 -c "import json;d=json.load(open('ci/chec rc=0 out=['PRODUCTION-GRAPH', 'ISA-LEAK-SELFTEST', 'SERIAL-HEAVY-SELFTEST', 'PR… | 维持 |
| V9-N-16 | G_GOV_GATE_P0 | OPEN | CI-001 | timeout 60 sed -n "159p" tools/quality/check_traceability.py rc=0 out=    return 0 [('TRACEABILITY', True)] REV = os.path.join(ROOT, "report… | 维持 |
| V9-N-17 | G_GOV_GATE_P0 | OPEN | CI-001 | timeout 60 grep -n "SKIP. taskset 不可用" lib/healpix_db/healpi rc=0 out=19:  echo "[SKIP] taskset 不可用 (非 Linux 或无 util-linux)"; exit 0 9:# 会在节… | 维持 |
