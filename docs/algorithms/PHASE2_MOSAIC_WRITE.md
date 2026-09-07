# Phase2 HiPS Mosaic Write Algorithms (ALG-P2-HIPS-001..004)

> ID: ALG-P2-HIPS-001..004  状态: FROZEN（P2-HIPS-DOC 冻结，2026-09-07）
> 上游 SCI（只读引用，全部 FROZEN，共享引用不改动）: SCI-UPM-001（docs/science/PHASE2_UPM.md §5 w_UPM 公式）、
> SCI-INT-001（docs/science/INTEGRATION.md §5 signal/sup_max 公式）、SCI-REJ-001（docs/science/REJECTION.md，
> SCI-REJ-001..008）、SCI-SCOPE-001（docs/science/SCIENCE_SCOPE.md）。共享 SCI 不因本任务改动；
> 本文件登记实现级语义（P1-WCS-DOC 共享 SCI 先例：SCI 公式语义不在本文重复定义，两处冲突以
> docs/science/ 为准并回改本文档，禁止反向）。
> 实现源（唯一权威生产源）: lib/phase2/tools/stage2.cpp（1762 行实测；入口 main :112）。
> 公式与默认容差以 SCI 层为权威，本文只登记实现锚点与实现自带语义；
> 本任务 no root science formula change（w_UPM / signal / sup_max / rejection 判据一律不改）。
> 下游: DATA-P2-INT / DATA-P2-RES（DATA_SEMANTICS §20 DATA-P2-HIPS——P2-HIPS-DOC 新增）、
> API-P2-001（docs/api/PHASE2_API_V1.md）+ PUBLIC_API Phase2 mosaic write 节（P2-HIPS-DOC 新增）、
> 模块 MOD-astrocs-phase2-hips-writer（registry 现状实测 astrocs.phase2.write.md =
> MOD-astrocs-phase2-write，二者对齐归 P2-HIPS-INT，见 §11.6）。
> 矩阵行: docs/traceability/TRACEABILITY_MATRIX.json（module_id=astrocs.phase2.write 现状域）。
> 纪律声明: 本文档由 P2-HIPS-DOC 冻结；所有 stage2.cpp:NNN 行锚逐条 grep/sed 实测；
> 未持有源码锚的语句不得声称 IMPLEMENTED；缺陷以 DISP-P2HIPS-* 登记，不反向修改 SCI。

## 1 上游 SCI 与输入输出

- SCI-UPM-001（PHASE2_UPM.md §5）: `w_UPM = quality_factor × geometric_reliability ×
  control_ivar`（:46 权威行）。stage2 侧实现锚 = UPM 构建消费该权重语义
  （`p2_upm_build_geo` :432-433），本文不改公式，只登记其进入马赛克链的编排位置。
- SCI-INT-001（INTEGRATION.md §5）: `signal` = accepted 样本加权积分输出（ADU）；
  `sup_max = max(accepted support)` canonical reducer（:21/:75）。stage2 侧实现锚 =
  `p2_integrate_pixel` 输出 `pr.signal/pr.support`（:1222-1223/:1524-1527，
  注释 "support 唯一 canonical reducer（max accepted support）由 p2_integrate_pixel
  计算，Stage2 只消费" :1525-1526），writer 视图换算 `flux=signal×area`、
  `area=support×A_cell`（:1227-1228/:1531-1532，见 §2）。
- SCI-REJ-001（REJECTION.md SCI-REJ-001..008，T107 冻结）: rejection 判据权威。
  stage2 侧只做计划解析与 kernel 调度（§2），不改任何判据。
- SCI-SCOPE-001: 处理链位置权威（Phase2 末节点 coverage→…→马赛克写出）。
- 输入: N 个 Phase1 单帧 HiPS 目录（`cfg.hips`），每帧消费 signal/support 两数据集
  （`aio_hips_open(AIO_HIPS_RD_SIGNAL/:AIO_HIPS_RD_SUPPORT)` :536-537）+
  weight_mode=2 时 ivar 数据集（`AIO_HIPS_RD_IVAR` :557）；SNR Catalogue 仅供
  frame 级 median fallback（`frame_snr_medians` :74-95，`AIO_HIPS_RD_SNR` :77，
  注释 "禁止重新检测星点" :73）。
- 输出: 单个 Phase2 马赛克 HiPS 目录（`cfg.out_hips`），仅 signal/support 两产品
  （flags=`AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT` :594；DISP-P2HIPS-001），
  creator="ivo://astrocs/phase2"、title="AstroCS Phase2 Mosaic"（:595）、
  filter=infos[0].filter_passband（:531/:596）；diagnostics=true 时另写
  diagnostics.json（:1748-1750）与 upm_sparse.json/upm_dense.cache（:472-483）。
- 下游消费: 编排层 p2_session（lib/phase2_session/p2_session.cpp:81-92 仅验证
  hips_paths/输出目录键，不做 HiPS 写）；P3 重采样经 aio_hips_reader（P3 域）；
  HIPS_VERIFY 为 stage2 自回读（§2 ALG-P2-HIPS-004）。

## 2 离散公式（锚=stage2.cpp 实测行号，禁止改写）

- **target_order 决议与禁止插值伪装分辨率**: `target_order =
  cfg.target_order ≥ 0 ? cfg.target_order : cov.target_order`（:203-204）；
  `target_order > cov.target_order`（高于输入最高 order）→ 显式拒绝 rc=3，
  log "target_order 高于输入最高 order，禁止插值伪装分辨率"（:205-208）。
- **输入哈希链**（ALG-P2-HIPS-001 专有语义）:
  - `fid_i = p2_frame_id(path_i)`（:222，缓存 :219-229）；`fid_i == 0` → rc=4
    （:223-228，"frame_id 0 invalid: … (p2_frame_id failed: hash/open exception)"）。
  - manifest entry = `fid_i | filter=<filter_passband>;order=<max_leaf_order>;frame=<frame_type>;`
    （:230-236，字段取自 P2HipsInputInfo）。
  - 按 frame_id 升序排序（:238-239）后串接 canonical payload
    `"<fid>|<meta>;"`（:240-242）；
    `input_manifest_hash = astrocs::crypto::sha256_hex(manifest_payload)`
    （:243-244，log :245）。
  - 该 hash 经 `p2_stage2_make_upm_cfg(cfg, target_order, input_manifest_hash.c_str())`
    （:428-430）进入 UPM 构建 → `minfo.model_hash`（:439-444，log "hash=…12 位…"）；
    diagnostics=true 时经 `p2_upm_save` 持久化（:449 分支，:473 调用），
    model_hash 写 diagnostics.json `model_hash`（:1746）。
    现状: 两 hash 均未进入 HiPS products 元数据（DISP-P2HIPS-002）。
- **球面常量**（:525-528）: `nside = 1 << (target_order + 9)`（:525）；
  `n_leaf = 512×512 = 262144`（:526）；
  `A_cell = 4π / (12·nside²)`（:527-528，float64 常量 3.14159265358979323846）。
- **产品 dtype**: `dtype = cfg.precision ? AIO_HIPS_FLOAT64 : AIO_HIPS_FLOAT32`
  （:529）。
- **tile 循环**（ALG-P2-HIPS-002，:659-660 `for ci in 0..cov.n_union_cells`，
  固定 tile 序 = coverage union cell 升序，确定性来源之一）:
  - 覆盖帧探测: 逐帧 `aio_hips_read_tile_f32(sig[f], tile_ipix, t_sig_probe)`
    probe（:663-668，读失败即该帧不覆盖此 tile），空帧集 `continue`（:669），
    `depth = |frames|`（:670）。无 MOC 缓存，逐 tile 逐帧重复 FITS 读
    （DISP-P2HIPS-004）。
  - rejection 计划解析（不改判据，只把 cfg 组装为 P2RejectionPlan）:
    `wbpp_2_9_1` → group-level 一次解析（:639-658，`p2_reject_plan_resolve`
    :650，nominal_contributors=帧总数 :646，tile 不重选 :675 采纳 group_plan）；
    `astrocs_adaptive` → tile 级按 nominal geometric depth 解析（:677-690）；
    normalization 三态映射（:691-696）+ floor（:697）；typed params 逐字段
    注入（sigma/winsorized/averaged/linear_fit/esd/percentile/median_sigma/
    minmax，:698-720）；`large_scale_rejection.v1` 参数注入（:721-729）；
    逐 tile 语义 id log（:730-738），`resolved_methods[depth] =
    p2_rejection_semantic_id(rplan.method)`（:739）。
  - ACR-CPU 路由（CON-007）: `use_acr_block =
    p2_acr_block_eligible(cfg, acr_reg != nullptr, rplan.method,
    large_scale_active)`（:744-746；注释 :741-743 "仅显式 sigma（robust_mad_clip）
    可走 ACR 块路径；large_scale 激活时强制 CPU"——kernel 注册查找
    `kOpMosaicReject` :625-627）；GPU 经 CUDA bridge 懒加载（:747-759，
    `executor_create` :751）；fallback_reason 规则（requested=auto →
    "linux_no_cuda_auto_fallback"；requested=cuda → "cuda_unavailable_fallback"；
    :761-769）；CON-007 route log requested/effective/workers/fallback_reason
    （:771-776）。
  - micro-chunk 内存规划: 逐 tile 真实 depth 重算 `p2_block_plan`
    （bp: output_pixels=n_leaf :780、covering_frames=depth :781、
    memory_limit_bytes=cfg.memory_limit_mb :783、safety_factor=0.75 :784、
    scratch 8B/sample+8B/pixel :785-786、fixed_overhead 1<<22 :787；
    plan.status==1 → "block unfeasible" rc=6 :790-794）；
    `chunk_pixels = min(n_leaf, max(1, plan.block_pixels))`（:795-797）、
    `n_chunk = ceil(n_leaf/chunk_pixels)`（:798-799）、working_bytes log（:800-817）。
- **ACR 块路径**（:863-1058，与 CPU reference 同一科学合同，路由仅执行域）:
  - weight_compact 8×8 网格（:864-899）: weight_mode=2 → local_ivar_map
    per-cell ivar（:877-884，缺失 key 时 `(ivar_product_missing==0) ? 1.0 : 0.0`）；
    mode 0 → local_snr_map + frame_snr fallback（:886-894）。
  - 逐 chunk 逐帧读 tile + UPM 空间校准 `p2_upm_calibrate_block`（:911-937，
    调用 :927-930，chunk_leaves=全局叶 ipix `(tile_ipix<<18)+local_lut[i]`
    :825-836）。
  - KernelInvocation（:942-988）: id=`kOpMosaicReject`（:943）；buffer
    0=out_sig(Output) / 1=frames(Input) / 2=support(Input) /
    3=weight_compact(Input) / 4=out_sup(Output) / 5=out_rej(Output) /
    6=out_valid(Output)（:945-959）；scalars = cnt、depth、rplan.method、
    underdetermined_n、sigma lower/upper/max_iterations、chunk 偏移 p0、
    weight_mode、acr_workers（:960-977）；cuda 可用走 `acr_reg->cuda`
    否则 `legacy_parallel`（:979-983），异常时 fallback CPU（:984-988）。
  - 输出逆变换（writer 视图）: `valid = out_sup > 0`；
    `area = out_sup × A_cell`；`flux = out_sig × area`（:989-1004，
    area :994-995、flux :996-997；precision 选择 f64/f32 缓冲 :998-1004）；
    诊断计数（:1005-1021）。
- **序转换合同（HIPS-IMG-001，两条写路径各一次）**: 注释原文（:1024-1027）
  "writer 约定 view 缓冲为 NESTED local 序…stage2 集成缓冲为 FITS 行主序，
  写入前转换 buffer[i]=buf[fits_index(i)]，否则 tile 内像素被散射错排
  （表现为 16px 周期 comb/重复星点）"；实现 `flux_leaf[i] =
  buf[nested_local_to_fits_index(i, 9, 512)]`（ACR 侧 :1028-1040；CPU 侧
  :1606-1619；签名 lib/common/healpix/healpix_core.h:43）。
- **CPU reference 权威路径**（ALG-P2-HIPS-003，:1061-1541）:
  - ivar 逐帧读取缓冲: `ivarv[depth×chunk_pixels]`（:1065）+
    `ivar_valid[depth]` per-frame-slot（:1066）；逐帧
    `aio_hips_read_tile_f32(ivr[f], …)` 成功才置 `ivar_valid[s]=1` 并按
    chunk 段拷贝（:1254-1263）。
  - eligibility 统一 collector: `P2EligibilityGatherInput{values(cal),
    support(supv), frame_ids(frame_seq), pixel, support_threshold=0}` →
    `p2_collect_candidate_stack`（并行 :1084-1105 / 串行 :1330-1353），
    `gout.source_indices` 为 eligible→原 frame slot 稳定映射
    （:1344-1346 注释 "compact eligible → 原始 frame slot 稳定映射"）。
  - 权重模式（并行 :1106-1140 / 串行 :1354-1400）:
    mode 2（默认）= 逐像素 ivar `weights[s] = ivarv[orig×chunk_pixels+i]`
    （ivar==0 合法零权重 ZERO_VALID_WEIGHT，注释 :1366-1368；nonfinite →
    `p2_validate_candidate_weights` 拒绝 hard fail，合同注释 :1366 "nonfinite
    ivar → INVALID_INPUT（validator 拒绝）"；缺 ivar 产品仅在显式 fallback
    路径可达（打开时已 gate :565-577），降级 support 并计数 :1372-1375）；
    mode 0（legacy/诊断）= `support × snr²`（local_snr_map 64×64 cell，
    key=(fid,tile_ipix,px/64,py/64)，:1377-1397，注释 :1357 "仅 ablation/
    诊断"）；mode 1 = 等权（:1398-1400）。validator 失败 → 诊断透出
    首 tile/像素 + rc=6（:1402-1431；并行版 :1141-1167 fail 原子位）。
  - rejection: `p2_reject_stack_ex(&cstack, &rplan, &rdec)`（:1441-1452）；
    仅 `P2_STATUS_OK / P2_STATUS_UNDERDETERMINED` 可继续，其余 hard fail
    （:1454-1461）；accepted 掩码 = `reasons[s] ∈ {P2_REASON_ACCEPTED,
    P2_REASON_UNDERDETERMINED}`（:1462-1467）。
  - 积分: `p2_integrate_pixel(&pi, &pr)`（:1515-1522）→
    `signal_out=pr.signal`、`support_out=pr.support`（:1524-1527）。
  - 逆归一（writer 视图换算）: `area = support_out × A_cell`、
    `flux = signal_out × area`（:1531-1532）；注意 signal/support 本身为
    SCI-INT §5 语义，`flux_sum/covered_area` 为 writer 视图约定
    （aio_hips_writer.cpp:476-479 `sig=flux/area, sup=area/A_cell 钳 1.0`
    逆过程，P2 共用该 writer 归一公式 ALG-HIPS-002）。
  - OMP 并行（CON-006，:1279-1322）: 仅 `!large_scale_active &&
    workers>1`（:1280）；`#pragma omp parallel num_threads(workers)` +
    `schedule(static)`（:1288/:1298）；per-worker 独立 scratch
    （stack/weights/acc/fid_stack/reasons/src_idx + per_thread_hist，
    :1290-1297 注释 "禁止像素热循环创建 vector"）；定序归并 = thread id
    固定顺序合并 map/计数（:1310-1317 注释 "定序归并：thread id 固定顺序，
    map/计数合并 deterministic"）；cpu_fail → rc=6（:1318-1322）。
  - large_scale 两遍（:1544-1605）: 第一遍仅缓冲 per-frame 值/权重/support/
    low/high mask（:1468-1503）；`p2_large_scale_apply(buf_lo, buf_hi, 512,
    512, nb, &rplan.large_scale)` connected-component grow（:1548-1554）；
    第二遍 "拒绝 mask 应用回原始 calibrated 科学值" 重收集 accepted
    （:1556-1575）+ 二次积分（:1579-1598）；grown 统计 log（:1599-1604）。
- **tile 序转换写出**（ALG-P2-HIPS-004，:1606-1635）: 序转换（:1606-1619）→
  `AstroSphereTileView{parent_ipix, leaf_order=target_order+9, width=512,
  data_type=dtype, flux_sum, covered_area, valid_mask}`（:1620-1628）→
  `aio_hips_write_signal_support_tile(ps, &view)`（:1629）失败 →
  `aio_hips_abort(ps)` + rc=6（:1630-1633）→ `++tiles_written`（:1635）。
- **finalize 与收尾**: `aio_hips_finalize(ps)` 失败 rc=6（:1638-1643）；
  逐帧关闭 sig/sup/ivr（:1645-1649）；mosaic written log（:1650-1656）。
- **HIPS_VERIFY**（:1659-1676）: `aio_hips_open(out_hips, AIO_HIPS_RD_SIGNAL)`
  回读失败 rc=7（:1661-1666）；`aio_hips_tile_count` 逐产品回读 log
  （:1667-1675）。
- **diagnostics.json**（:1684-1751，写 :1748-1750）: acr_requested/effective_route
  （:1698-1699）、acr_workers/fallback_reason（:1700-1701）、component_count
  （:1703）、tiles_written/output_pixels/rejected_samples/fallback_pixels
  （:1708-1711）、pixels_depth_0/1/ge_2（:1712-1714）、
  rejection_resolved_methods（depth→semantic id，:1741-1744）、
  rejection_samples_per_pixel（reject_hist，:1745）、model_hash（:1746）、
  runtime_seconds（:1747）。

## 3 伪代码（stage2.cpp main :112-1762 结构直译）

```text
main(stage2.json, CLI overrides):
  argc/config 解析失败 → rc=2                        # :130-154
  CLI override --cpu-workers/--io-workers/--gpu-route/--deterministic (CON-002)  # :155-166
  日志 run/logs/phase2/<date>/stage2.log             # :168-173
  # ALG-P2-HIPS-001 编排生命周期
  coverage 两阶段（容量+填充, p2_coverage_build）失败 → rc=3   # :189-202
  target_order 决议；高于输入最高 order → rc=3（禁伪装分辨率）  # :203-208
  for f: fid=p2_frame_id(path); fid==0 → rc=4;        # :221-229
        manifest_entry = fid|filter=;order=;frame=;    # :230-236
  sort by fid → canonical payload → input_manifest_hash=sha256_hex  # :238-245
  CONTROL_SAMPLE 两阶段（p2_sample_controls_cached）失败 → rc=4  # :279-320
  UPM_FIT: make_upm_cfg(hash) → p2_upm_build_geo 失败 → rc=5    # :427-438
  UPM_PERSIST (diagnostics): upm_save + dense cache → 失败 rc=5 # :447-499
  BLOCK_PLAN（全局估算 tile_pixels=262144）失败 → rc=6          # :502-521
  # ALG-P2-HIPS-002 tile 循环
  nside=1<<(target_order+9); n_leaf=512×512; A_cell=4π/(12·nside²); dtype  # :525-529
  for f: open signal+support（失败 rc=6）; weight_mode=2 时 open ivar       # :533-548
  ivar 门: 缺失>0 且 !legacy_allow_weight_fallback → rc=7        # :565-574
  out_hips 存在且非目录 → rc=6;                                  # :586-589
  aio_hips_product_begin(signal|support, creator, title, filter) # :592-596
  for ci in 0..cov.n_union_cells:                                # :659
    probe 覆盖帧（read_tile_f32）→ frames; 空 → continue         # :663-669
    解析 rejection 计划（group-level 或 tile 级）+ typed params   # :672-739
    ACR 路由（p2_acr_block_eligible / CUDA bridge / fallback_reason）  # :744-776
    micro-chunk 内存规划（p2_block_plan; unfeasible → rc=6）     # :779-817
    if use_acr_block:                                            # :863
        weight_compact 8×8 ← local_ivar_map / local_snr_map      # :864-899
        for chunk: 读帧+UPM 校准 → KernelInvocation(kOpMosaicReject)
                   → area=out_sup×A_cell, flux=out_sig×area      # :905-1021
        # 写出（序转换 → view → write）见下，continue             # :1058
    # ALG-P2-HIPS-003 CPU reference 权威路径（micro-chunk）
    for chunk:                                                   # :1239
        for s in frames: 读 sig/sup + ivar; UPM 校准 cal          # :1245-1277
        if OMP && !large_scale && workers>1:                      # :1280
            parallel for pixel: per-worker scratch
            eligibility → weights(mode2=ivar/0=sup·snr²/1=等权)
            → validate(hard fail rc=6) → reject_stack_ex
            → accepted(reasons) → integrate_pixel
            → area=sup×A_cell, flux=sig×area                      # :1299-1237
            定序归并（thread id 顺序）                             # :1310-1317
        else: 串行同语义（含 large_scale 缓冲分支）               # :1326-1541
    if large_scale: p2_large_scale_apply(grow) → 二次积分         # :1544-1605
    # ALG-P2-HIPS-004 写出与收尾
    buffer[i]=buf[nested_local_to_fits_index(i,9,512)]            # :1606-1619
    AstroSphereTileView → aio_hips_write_signal_support_tile（失败 abort+rc=6）
    # :1620-1635
  aio_hips_finalize（失败 rc=6）→ 关闭输入                        # :1637-1649
  HIPS_VERIFY: 回读 signal（失败 rc=7）+ support tile count       # :1659-1676
  diagnostics.json（可选）                                        # :1684-1751
  return 0（unhandled exception → rc=1）                          # :1752-1761
```

## 4 协议/内存

- 单进程 CLI 生命周期: 一次 `main` 内完成 DISCOVER→VALIDATE→
  COVERAGE_UNION→CONTROL_SAMPLE→UPM_FIT→UPM_PERSIST→BLOCK_PLAN→
  REJECT+INTEGRATE+HIPS_WRITE→HIPS_VERIFY（头注释 :5-8；阶段日志锚
  :173 start、:186 coverage 注释、:255 CONTROL_SAMPLE 注释、:427 upm_fit、
  :447 upm_persist、:500 block_plan、:524 reject+integrate+hips_write、
  :1659 HIPS_VERIFY 注释）。UBO 模型由 `P2ModelInfo.model_hash`（:439-444）
  与 diagnostics `model_hash`（:1746）登记，非本文合同域。
- 阶段间数据传递为进程内 vector/POD 借用（cov/obs/ctrl_nodes/model），
  无跨进程序列化合同；UPM 持久化（upm_sparse.json + upm_dense.cache）
  仅 diagnostics 路径（:449-495）。
- 并发合同: writer 单句柄串行（P1-HIPS-DOC ALG-HIPS-001..005 域合同，
  aio_hips_writer.cpp；DISP-HIPS-006 单互斥锁缺口在 P1 域登记）；
  stage2 像素并行仅 CPU reference 内部（CON-006，:1279-1322），OMP
  worker 不触碰 `ps` 写句柄——`aio_hips_write_signal_support_tile` 仅在
  tile 循环主线程调用（:1050/:1629）；统计归并以 thread id 定序归并保证
  deterministic（:1310-1317）。日志 mutex（g_log_mutex :62，log :64-69）。
- 内存: tile 级缓冲全部局部 vector（valid/fluxD/areaD/fluxF/areaF :615-617、
  probe :618、cal/supv :821-822、large_scale 缓冲按 nb×n_leaf cap
  :849-861）；UPM model 句柄由 `p2_upm_close` 统一释放（各 rc 出口 :455/
  :476/:487/:516/:545/:573/:588/:600/:652/:686/:792/:918/:1054/:1251/:1320/
  :1429/:1451/:1459/:1552/:1632/:1641/:1664/:1678）。
- 退出码表（实测）: 2=config/CLI（:133/:140/:146/:153/:160-165）、
  3=coverage/target_order（:195/:201/:207）、4=frame_id/sampler（:227、
  :283/:288/:292/:298/:303/:310/:315/:319）、5=UPM build/persist（:437/
  :456/:477/:488）、6=写路径/块不可行/validator（:517/:546/:589/:601/:793/
  :918/:1055/:1252/:1321/:1430/:1452/:1460/:1553/:1633/:1642）、
  7=ivar 门/HIPS_VERIFY（:574/:1665）、1=unhandled exception（:1756/:1760）。

## 5 边界/NaN/Inf/invalid

- 空帧集 tile: probe 全失败 → `continue`（:669），不写该 tile。
- 输入打开失败: signal/support 任一失败 → 关闭已开句柄 + rc=6（:538-547）。
- ivar 门（weight_mode=2 默认）: 逐帧 `AIO_HIPS_RD_IVAR` 失败计数
  ivar_product_missing（:553-563）；>0 且 `legacy_allow_weight_fallback=false`
  （默认）→ 显式科学错误 rc=7，log "拒绝继续，防止在非逆方差语义下冒充
  ivar coadd"（:565-574）；显式 true 才降级 support 并 diagnostics 标红
  （:576-577）。
- ivar 数值: ivar==0 合法零权重（ZERO_VALID_WEIGHT，不贡献，:1366-1368）；
  nonfinite/负 → `p2_validate_candidate_weights` hard fail rc=6（:1402-1431），
  禁止静默换 support（:1369 注释 "禁止静默换 support"）。
- 零权重像素: n_valid==0 → px_depth_0 计数，valid=0（:1434-1436/:1225-1231）；
  UNDERDETERMINED → total_fallback/underdetermined_px（:1507-1514）。
- support==0 → valid=0、area/flux=0（ACR 路径 :992-997）。
- output path: out_hips 存在且非目录 → rc=6（:586-589）；磁盘可用量仅 log
  告警不阻断（:583、UPM persist 侧 :458-470）。
- writer 层 NaN/Inf 防护: `std::isfinite(flux)&&std::isfinite(area)` 才累加
  （aio_hips_writer.cpp:477，P1-HIPS-DOC 域，本模块消费该合同）。
- large_scale: apply 失败 rc=6（:1551-1553）；pre/post rejected 统计差 =
  grown（:1599-1600）。

## 6 复杂度与误差来源

- 时间: probe O(T·N) 次 FITS tile 读（T=tiles_written 域内 tile 数，N=帧数，
  :663-668，DISP-P2HIPS-004）；每 tile 每帧再读 1-2 次（chunk 循环内
  :1247-1249，ivar +1 次 :1256）；集成 O(T·n_leaf·depth·(rejection 方法成本))；
  OMP 并行仅在 CPU reference 非两遍路径（:1280）。
- 内存: large_scale 激活时 cap=nb×n_leaf×(8+8+8+1+1+1)B + n_leaf×4B
  （:852-861）为 tile 级峰值项；否则 O(depth×chunk_pixels)。
- 误差来源: f32 产品下 flux/area 经 `(float)` 截断入 writer 视图（:1002-1003/
  :1537-1538/:1033-1038），与 writer 侧 f32 累加漂移（DISP-HIPS-009，P1 域）
  同向叠加；f64 产品（cfg.precision）无此层截断。A_cell 常量双精度，无附加
  误差。UPM 校准引入模型自身误差（UPM 域，非本模块）。
- 确定性: 固定 tile 序（cov.n_union_cells 升序 :659-660）+ 固定 chunk 划分
  （:795-799）+ schedule(static)（:1298）+ thread id 定序归并（:1310-1317）
  + 单线程 writer 调用（:1629）→ 同输入同 config 同 mosaic（§7）；实证
  Phase2IvarWiring 1T/2T/repeat 差分数==0（ivar_wiring_test.cpp:314-316/
  :339-341，容差阈值 1e-4/1e-6 下零差异像素）。

## 7 合同负向条款（科学红线，P2-HIPS 专项）

- **四概念分离红线**: `signal`（SCI-INT §5 加权积分输出，ADU）、
  `variance/ivar`（输入侧逐帧产品消费，w_i=ivar_i，weight_mode=2 默认），
  `support`（SCI-INT §5 sup_max=max(accepted support)，几何覆盖 [0,1]，
  A_cell 归一）、`mask`（rejection reasons→accepted 掩码，large_scale grow
  后处理）在缓冲/产品/命名上严格分离；**禁止 support 当科学权重冒充
  ivar**——weight_mode=2 缺 ivar 产品 → rc=7 或显式标红 fallback（:565-577），
  像素级缺 ivar 仅显式 fallback 路径可达并降级 support 计数（:1372-1375）；
  **禁止 valid_mask 入权重式**（valid_mask 仅 writer 视图层
  `view.valid_mask` :1628，权重式只取 ivar/support）。
- **no root science formula change**: w_UPM（PHASE2_UPM.md §5）、signal/sup_max
  （INTEGRATION.md §5）、rejection 判据（REJECTION.md SCI-REJ-001..008）
  一律不改；本模块实现锚只登记 stage2 侧编排语义（§1 纪律声明）。
- **禁止插值伪装分辨率**: target_order ≤ 输入最高 order，违者 rc=3
  （:205-208）。
- **输出确定性**: 同输入同 config → 同 mosaic（固定 tile 序 :659、固定
  chunk 划分 :795-799、OMP 定序归并 :1310-1317、单线程 writer 调用
  :1629）；UTC 时间戳字段（hips_release_date 等）除外（writer 层合同
  ALG-HIPS-005 域）。
- **并发合同**: writer 单句柄串行（parallel_ok=false；DISP-HIPS-006 单互斥锁
  缺口在 P1 域登记）；stage2 像素并行仅 CPU reference 内部（CON-006），
  不并发写 writer。
- **不做**: 不改 P1 域 writer 归一公式（ALG-HIPS-002 消费）；不做 P3 重采样
  （aio_hips_reader 为 P3/HIPS_VERIFY 后端）；不做编排（p2_session.cpp:81-92
  仅路径验证）；不新增产品（仅 signal|support :594，DISP-P2HIPS-001）；
  不在本模块内实现原子发布（DISP-P2HIPS-003，归 IO-003 编排层）。

## 8 参考实现/Oracle

- 权威参考实现 = stage2.cpp CPU reference 权威路径本身（:1061-1541，
  "CPU reference 路径" 注释 :1061）；ACR 块路径为执行域加速（:863-1058），
  两者科学语义等价合同（kernel legacy_parallel 同路径 :979-983；ACR 等价
  权威 = docs/algorithms/ACR_EQUIVALENCE.md ALG-ACR-EQUIV）。
- Oracle 设计（独立于被测符号，不复制 §2 公式）:
  - Python/NumPy 参考实现: 合成 3 帧单 tile HiPS（signal/support/ivar），
    按 SCI-INT §5 语义独立复算 signal（weighted mean, w=ivar）与
    sup_max=max(accepted support)，与生产输出比对 rtol=1e-12（f64 产品）
    ——现状 MISSING，见 §9。
  - 序转换往返恒等: `nested_local_to_fits_index(i,9,512)` ↔
    `fits_index_to_nested_local`（stage2.cpp:825-827 消费后者）对全
    262144 索引双射断言（整数精确，无容差）。
  - ivar 门负测: weight_mode=2 且 ivar 产品缺失、legacy_allow_weight_fallback
    未显式置 true → rc=7（:565-574）；置 true → rc=0 且 diagnostics
    ivar_product_missing>0。
- 既有可执行测试（legacy gate，迁移基线，实测）:
  lib/phase2/tests/ivar_wiring_test.cpp
  `Phase2IvarWiring.WireProductionStage2PerFrameIvar`（:223 起）——
  直接跑生产 astrocs-stage2（:3 注释，:147 run_stage2），3 帧合成
  signal/support/ivar，验证 WIRE-IVAR-001..005（per-frame ivar 接线、
  invalid compact 不错位、期望 weighted mean 匹配、C 帧 ivar×4 局部生效、
  帧置换不变）+ CON-006 1T/2T 逐层差分（:304-322）+ CON-009 repeat-2T
  bitwise（:324-345）；lib/phase2/tests/routing_test.cpp
  `Phase2Routing.AcrCpuRouteStaysCpuNoSilentGpu`（:54）/
  `AcrCpuRouteEntersCpuAcrBlockForLegacyWeightMode`（:65）；
  tests/backend/test_p2004_reject_integrate.py（cosmic ray rejected/
  auto resolves/integration weighted mean/all rejected，:152-170）。

## 9 容差来源

- 科学数值容差唯一权威 = SCI 层默认容差（INTEGRATION/REJECTION 冻结值）；
  本文只登记测试实测阈值:
  - Phase2IvarWiring: 1T/2T 图层差分阈值 1e-4、差数必须==0
    （ivar_wiring_test.cpp:311-322）；repeat-2T 阈值 1e-6、差数==0
    （:339-341，by-construction 位精确注释 :327-328）；帧置换/ivar 局部性
    阈值 1e-4（:383-385/:406-410）。
  - ACR↔CPU 等价: ALG-ACR-EQUIV（docs/algorithms/ACR_EQUIVALENCE.md）
    权威容差域。
  - 集合/索引运算整数精确: target_order/ipix/序转换往返容差=0（bitwise）。
- **TEST-P2-HIPS-001 MISSING**: 专属端到端马赛克 synthetic gate（NumPy
  signal/sup_max oracle rtol=1e-12 + 序转换全索引双射 + ivar 门负测 +
  diagnostics 字段断言）现状缺失——上述 Phase2IvarWiring/Wiring 域测试
  覆盖 ivar 接线与确定性，但不覆盖: f64 产品数值 oracle、多 tile、
  large_scale 两遍、ACR 块路径与 CPU reference 数值等价。TEST-DESIGN
  建议（TEST-P2-HIPS-001，命名沿用 PHASE2_COVERAGE.md §8/§11.4 先例）:
  F1 NumPy 参考复算 signal/sup_max（f64，rtol=1e-12）；F2 序转换往返
  恒等（bitwise）；F3 ivar 门负测（rc=7/显式 fallback 诊断）；F4
  large_scale 两遍（grow 后二次积分=手工 grow 复算）；F5 ACR vs CPU
  同输入同 config 输出 rtol=1e-12（f64）；F6 退出码表负测（§4）。

## 10 关联 ARC/API/TST

- 上游 SCI: SCI-UPM-001（PHASE2_UPM.md §5）、SCI-INT-001（INTEGRATION.md
  §5）、SCI-REJ-001（REJECTION.md，SCI-REJ-001..008）、SCI-SCOPE-001
  （SCIENCE_SCOPE.md）——全部共享只读引用，不改动（§引言纪律声明）。
- ALG 上游: ALG-UPM-001（docs/algorithms/UPM_SOLVER.md）、
  ALG-REJ-001..008（docs/algorithms/REJECTION_ALGORITHMS.md，DERIVED T207
  冻结）、ALG-COV-001（docs/algorithms/PHASE2_COVERAGE.md，P2-COV-DOC
  冻结）、ALG-HIPS-001..005（docs/algorithms/HIPS_WRITER.md，writer 库
  lib/astro_image_io/src/hips/aio_hips_writer.cpp——P2 马赛克共用其
  ALG-HIPS-002 归一公式 signal=flux_sum/covered_area、
  support=covered_area/A_cell 钳 1.0（:477-479）与 finalize manifest.json
  语义（:1086-1128）；variance 产品（:1060-1066）P2 不启用；P2 登记的
  是 stage2 侧编排与集成语义，writer 域合同不在此重登记）。
- DATA: DATA-P2-INT（integrated，registry astrocs.phase2.integrate.md:22）、
  DATA-P2-RES（mosaic，astrocs.phase2.write.md:23）；逐字段唯一权威见
  DATA_SEMANTICS §20（P2-HIPS-DOC 新增）。
- API: API-P2-001（docs/api/PHASE2_API_V1.md，FROZEN，所有权/并发合同）+
  PUBLIC_API Phase2 mosaic write 节（P2-HIPS-DOC 新增；现状 PUBLIC_API.md
  :54 登记 astrocs-stage2 CLI 为 V5 遗留 LEG-004 已退出生产，生产入口 =
  `astrocs phase2 run` 编排——本节登记其底层写出实现）。
- 模块: MOD-astrocs-phase2-hips-writer（registry 现状实测
  astrocs.phase2.write.md，module_id=astrocs.phase2.write，execution_class=io；
  对齐归 P2-HIPS-INT，见 §11.6）。
- IO: IO-002（docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md，读输入）、
  IO-003（docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md，发布层，
  DISP-P2HIPS-003 承接方）。
- 相邻不改: aio_hips_reader.cpp（P3/HIPS_VERIFY 后端）、
  lib/healpix_db/healpix_drizzle/astro_sphere_sink.cpp（P1 写通道）、
  p2_session.cpp（编排层，hips_paths 验证 :81-92，不做 HiPS 写）。
- TST: TEST-P2-HIPS-001（登记面 = 本文档 §11.4 设计冻结 VERIFIED，
  依 P2-COV-DOC TEST-COV-DESIGN-001 先例：VERIFIED 对象为设计+容差，
  可执行测试显式 MISSING 归 P2-HIPS-TEST）；既有基线
  Phase2IvarWiring/Phase2Routing/test_p2004_reject_integrate.py（§8）。

## 11 P2-HIPS-DOC 冻结附录（2026-09-07，stage2.cpp 1762 行源码实测）

### 11.1 ALG-P2-HIPS-001..004 逐符号/逐段锚

| ALG | 载荷 | 锚（stage2.cpp） |
|---|---|---|
| ALG-P2-HIPS-001 | 编排生命周期与输入哈希链 | main :112；config rc=2 :130-154；CLI override（CON-002）:155-166；coverage 两阶段 :189-202；target_order 禁伪装 :203-208；frame_id :221-229；manifest entry :230-236；sort+payload :238-242；sha256 :243-245；upm_fit 入口 :427-438；make_upm_cfg :428-430；model_hash :439-444；upm_persist :447-499 |
| ALG-P2-HIPS-002 | tile 循环（覆盖帧探测/计划解析/ACR 路由/micro-chunk） | tile 循环 :659-660；probe :663-669；group 解析 :639-658；tile 级解析 :675-690；typed params :698-720；large_scale 参数 :721-729；ACR eligibility :744-746；CUDA bridge :747-759；fallback :761-769；route log :771-776；block plan :779-817 |
| ALG-P2-HIPS-003 | 逐像素集成（eligibility→权重→排异→积分→逆归一） | ivar 缓冲 :1062-1066；ivar 读取 :1254-1263；collector 并行 :1084-1105/串行 :1330-1353；权重 :1106-1140/:1354-1400；validator :1141-1167/:1402-1431；reject_stack_ex :1184-1210/:1449-1467；integrate :1213-1223/:1515-1527；逆归一 :1227-1235/:1531-1539；OMP :1279-1322；large_scale 两遍 :1544-1605 |
| ALG-P2-HIPS-004 | 叶 tile 序转换写出、finalize、HIPS_VERIFY、diagnostics | 序转换合同 :1024-1040（ACR）/:1606-1619（CPU）；view :1041-1049/:1620-1628；write :1050-1056/:1629-1634；finalize :1637-1643；关闭输入 :1645-1649；HIPS_VERIFY :1659-1676；diagnostics :1684-1751 |

### 11.2 状态码/返回码语义

- 退出码表（唯一出口 main :112-1762）: 0=成功（:1752）；1=unhandled
  exception（:1756/:1760）；2=config/CLI；3=coverage/target_order；
  4=frame_id/sampler；5=UPM build/persist；6=写路径/块不可行/validator/
  reject kernel；7=ivar 门/HIPS_VERIFY（逐锚 §4）。
- 出口资源责任: 每 rc 出口先 `p2_upm_close(model)`（§4 锚清单）；
  tile 写失败另 `aio_hips_abort(ps)`（:1053/:1631）。
- 失败非原子: 任何 rc≠0 出口前已写 tiles 残留（stage2 无 staging，
  DISP-P2HIPS-003）。

### 11.3 DISP-P2HIPS-001..004 实现缺陷/偏差清单（登记不改码，整改归 P2-HIPS-IMPL/INT）

| ID | 严重度 | 描述 | 源码锚 | 整改去向 |
|---|---|---|---|---|
| DISP-P2HIPS-001 | 中 | 马赛克输出仅 signal/support 两产品（flags=AIO_HIPS_PRODUCT_SIGNAL\|AIO_HIPS_PRODUCT_SUPPORT），输入侧消费 ivar（weight_mode=2）但输出侧无 variance/ivar 产品——方差传播止于加权积分，无逐像素方差输出供下游（P3/统计）消费；writer 层 variance 通道（aio_hips_writer.cpp:1060-1066）未被 P2 启用 | stage2.cpp:594; :553-563; aio_hips_writer.cpp:1060-1066 | P2-HIPS-IMPL 评估 variance 产品接入（writer 侧已具备，属接线缺口非能力缺口） |
| DISP-P2HIPS-002 | 中 | input_manifest_hash 与 model_hash 仅进入 UPM 持久层与 diagnostics.json，未写入 HiPS properties/manifest.json——全文件 `aio_hips_set_drizzle_provenance` grep 零命中（实测 0 处），provenance 链断在 products 元数据层，跨 run 溯源依赖 run 目录约定 | stage2.cpp:245; :427-430; :1746; 全文件 grep 零命中 | P2-HIPS-INT 经 writer provenance 通道接线（aio_hips.h provenance API），不改 SCI |
| DISP-P2HIPS-003 | 低 | stage2 直写 cfg.out_hips（aio_hips_product_begin :592），无 staging 目录；原子发布语义依赖 IO-003 Python 发布层（docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md）在编排层承接，stage2 单体运行时无该保护；与 writer 层 DISP-HIPS-004 同源 | stage2.cpp:592; docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md | P2-HIPS-INT 编排层接线（与 DISP-HIPS-004 整改同域） |
| DISP-P2HIPS-004 | 低 | 覆盖帧探测逐 tile 逐帧 aio_hips_read_tile_f32 probe，n 帧×n_tile 次重复 FITS 读，大 N 输入时 I/O 放大（O(T·N) probe）；无 MOC 缓存探测 | stage2.cpp:663-668 | P2-HIPS-IMPL 引入逐帧 MOC/tile 集合缓存（coverage 层已有逐帧 tile 列表可复用，ALG-COV-001 输出） |

### 11.4 TEST-P2-HIPS-001 设计冻结（MISSING，可执行测试归 P2-HIPS-TEST）

见 §9 Oracle 设计与 F1-F6 负测/边界矩阵；容差冻结 = f64 oracle rtol=1e-12、
索引/集合 bitwise、1T/2T/repeat 差数==0（现状阈值 1e-4/1e-6 见 §9，为
f32 产品存取粒度所致，f64 oracle 不沿用）；fixture 生成器注记容差来源
（§9）。EVIDENCE 显式 MISSING，不冒认 IMPLEMENTED。

### 11.5 SCI 层状态声明（本任务零 SCI 改动）

- 覆盖链全部语义权威已有 FROZEN SCI: SCI-UPM-001（T106 2026-08-23）、
  SCI-INT-001（T108 2026-08-23）、SCI-REJ-001（T107 2026-08-23）、
  SCI-SCOPE-001。四者均**不因本任务改动**（共享 SCI 引用不改动；
  P1-WCS-DOC 共享 SCI 先例）。
- matrix P2 域 science_id 占位（registry astrocs.phase2.write.md:7
  upstream=SCI-P2-WR-001/ALG-P2-WR-001）无 docs/science 权威页：语义映射
  由本节声明——SCI-P2-WR-001 ⇒ 指向既有 FROZEN 共享 SCI（权威=INTEGRATION.md
  §5 + PHASE2_UPM.md §5 + REJECTION.md + SCIENCE_SCOPE.md，矩阵行
  P2-HIPS-DOC 冻结时修订）；ALG-P2-WR-001 ⇒ ALG-P2-HIPS-001..004（本文档
  §2/§7）。两处冲突以 docs/science/ 为准并回改本文档（禁止反向）。

### 11.6 与任务给定事实的实测差异记录（以实测为准，供 P2-HIPS-INT 对齐）

- 行号偏移（控制包给定 vs 实测）: config 解析 rc=2 :130-154（给定 :127-141）；
  CLI override :155-166（给定 :142-159）；禁伪装分辨率 :205-208（给定
  :199-204，该处为两阶段填充调用）；p2_acr_block_eligible 调用 :744-746
  （给定 :740-742 为注释行）；CUDA bridge :747-759（给定 :743-756）；route
  log :771-776（给定 :759-767）；tile 级 p2_block_plan :778-817（给定
  :785-802）；ACR KernelInvocation :942-988（给定 :950-995）；ACR 逆变换
  :989-1004（给定 :1000-1017）；CPU validator :1402-1431；输入关闭
  :1645-1649（给定 :1650-1656）；HIPS_VERIFY :1659-1676（给定 :1665-1683）。
  其余给定锚全部实测吻合。
- 模块 ID: MOD-astrocs-phase2-hips-writer 在 matrix/registry 无现状
  （实测 MOD-astrocs-phase2-write / astrocs.phase2.write.md，execution_class=io）。
- DATA_SEMANTICS 现状止于 §19（DATA-COV-001）；§20 DATA-P2-HIPS 为
  P2-HIPS-DOC 新增登记位（DATA-P2-INT/DATA-P2-RES 现定义于
  TRACEABILITY_MATRIX.json 与 registry astrocs.phase2.integrate.md:22/
  astrocs.phase2.write.md:23）。
- PUBLIC_API.md 现状无 Phase2 mosaic write 节，且 :54 登记 astrocs-stage2
  CLI 为 V5 遗留（LEG-004 已退出生产，生产入口 astrocs phase2 run）——
  本文档登记对象为该 CLI 背后的底层写出实现 lib/phase2/tools/stage2.cpp。
- 测试现状: 无名为 TEST-P2-HIPS-001 的测试；实测基线 = ivar_wiring_test.cpp
  （直接跑生产 astrocs-stage2）、routing_test.cpp、synthetic_gate.cpp
  Phase2Integrate/Phase2Robust（reducer 级，:2622/:3360）、
  tests/backend/test_p2004_reject_integrate.py。
