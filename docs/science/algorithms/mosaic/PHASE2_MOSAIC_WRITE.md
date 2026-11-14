# Phase 2 最终单 Signal HiPS Writer：算法、生产接口与 Export 科学身份

**D5 完整长卷语义重构（2026-10-17）**。全文复读 `63eccbb6133aef56389641d43c55df6e243819c5:docs/science/algorithms/mosaic/PHASE2_MOSAIC_WRITE.md` 原 524 行（blob `ade52b4f0f307c7503a29b224eb04c7bd81c7483`），对照当前 `module_adapters.cpp::p2_op_write`、AIO HiPS Writer 与 [Mosaic 模块](../../../detail/mosaic/modules.md#write)、[Export 模块](../../../detail/export/modules.md)。本篇保留原 ALG-P2-HIPS-001..004 的数值/布局/资源/边界/Oracle，但纠正其把旧独立工具 `coverage/tools/stage2.cpp::main` 与旧多通道能力冒充生产 CLI 的描述。生产代码审查基线 `322da061bd9f46cd001a5a45c0f226a05b57777b`，文档不代表 D5 新编译或真实运行。

## 1. 最终科学形状：唯一 Signal

Stage 2 产出的正式科学图像**只有 `signal/` 一个 HiPS 子产品**。不能将老文列出的 `signal+support+variance+ivar` 四层当作当前输出政策；内部 `p2_integrated.bin` 派生的 signal/support/wsum、可用 variance/ivar/拒绝掩膜、`p2_corrected`、`p2_rejection` 是**计算/统计和 provenance 载体**，不等于要落盘为第二、第三、第四个科学图像。Final `p2_final.json.products=[\"signal\"]`，单 signal 的 `properties`、tile、上层汇总和完成 manifest 形成同一产品 revision。

| 层/对象 | 当前角色 | 不可偷换的科学量 |
|---|---|---|
| Stage 1 每帧 `signal`，可能携伴随 `support/variance/ivar` | 真实逐帧输入源；P1 的 FrameScience-v2 另有阶段数据合同 | 每帧与自身 bandpass、光度尺度、噪声条件一一绑定 |
| Phase 2 `coverage → sample → sky-plane → corrected → rejection` | 中间几何、局部天空估计、校正、候选资格与排异 | support 是几何/有效区域信息，不是 1/variance；point SNR 不是背景精度 |
| Phase 2 `integrate` 产出 `signal/support/wsum` `float64` 工作数组与可用性标记 | 写端读取，必要时构造 `flux_sum/covered_area` 和内部方差分子 | `signal` 是 corrected accepted 后的**面亮度**，`wsum` 是估计量所用权重和，缺资格不是零天空 |
| Phase 2 `write` 发布 `signal/` | 唯一正式 HiPS 科学图像 + `p2_final.json`/manifest/provenance | 不另输出 support/variance/ivar 科学图层，含模型及统计未必就可恢复协方差 |
| Phase 3 `properties/resample/writer/verify` | 从完成 P2 Signal 导出一张平面 WCS FITS PRIMARY signal | Export 不重新拟合通带、天光或噪声，不从单图反求全部方差/有效光谱响应 |

用户可以自由选择合并不同设备与滤镜。最终信号可能是一个**混合响应约定下**的数值合成结果，而不是物理上拥有同一固定滤镜的天区测量；元数据如实声明这种限制，不因 filter 不同拒绝正常合成。与 `obs_filter` 同义的历史“同滤镜才能继续”是过时约束，不能进入正式生产。

## 2. 归约数值与保持光度量纲

对目标 HEALPix 叶 \(p\) 的已接受校正面亮度 \(x_{kp}=y_{kp}-\delta_k(p)\)、合法权重 \(w_{kp}\ge0\)，数值归约在 \(W_p=\sum_{\mathrm{accepted}}w_{kp}>0\) 时为
\[
 \widehat B_p=\frac{\sum_{\mathrm{accepted}}w_{kp}x_{kp}}{W_p},
 \qquad s_p=\max_{\mathrm{accepted}}s_{kp}.
\]
\(s_p\) 是 support reducer 的摘要，**不是**相加后物理精确覆盖并集；若 no candidates、全部无效、全排异、零有效正权或溢出，由 [INTEGRATION](../../integration/INTEGRATION.md) 的状态机显式区分。不能把 \(\sum w=0\) 的 NaN 改为 0，也不能将零权样本作精度贡献。点源 \(I_F=1/V_F\) 与面亮度 \(1/V_B\) 必须经同条件 transfer 才能换算；实际多响应/PSF 混合也会改变被估计量。

**写端编码恒等式**：令 \(\Omega_p=4\pi/(12N_{\rm side}^2)\) sr。当前 `p2_op_write` 从 integrate 文件读取 `sig_p=\widehat B_p`、`sup_p=s_p`、`wsum_p=W_p`，逐叶构造：
\[
 A_p=\begin{cases}s_p\Omega_p,&s_p>0\land{\rm finite},\\0,&\text{否则},\end{cases}\quad
 Q_p=sig_p\,A_p,\qquad B_p^{\rm write}=Q_p/A_p\quad(A_p>0).
\]
这里 `AstroSphereTileView.flux_sum` 承载 \(Q_p\)，`covered_area` 承载 \(A_p\)，AIO 最后以 \(Q_p/A_p\) 恢复 signal。**即使不写 support 子产品，`covered_area` 输入也不能删**，否则无法归一。面积单位 sr、输出 signal 单位 \(L/\mathrm{sr}\)；若标度是 ADU/sr，则 \(Q_p\) 为 ADU。当前 Writer 固定 `kP3BunitSurfaceBrightness` 一类字面量，必须同真实输入标度、P1 response-calibration 和模型 application ID 逐项复核；`ADU/sr` 标签不能自动证明电子、外定标 SI 通量的数值符合该单位。

在可用内部逐像素面亮度逆方差 \(W_p\) 确实同被估量时，形式方差 \(V_{\widehat B}=1/W_p\) 仅针对独立且权重正确的固定选择模型。历史代码的 var numerator 定义 \(N_{V,p}=A_p^2/W_p\) 用作 Writer 对照/运行诊断；当前 `AIO_HIPS_PRODUCT_SIGNAL` **不启用**额外图像产品，因此不可从 `p2_final.json.uncertainty_available=true` 推断存在 `variance/` tile。对 `A_p=0`、`W_p=0`、`W_p<0` 或非有限、以及带相关协方差的样本，必须按三态/损坏态分辨，不能默默 `1/0` 变无限权。真实输出统计方差（固定接受集合）为
\[
 \operatorname{Var}(\hat B_p)=
 \sum_{k,\ell} a_{kp}\,\operatorname{Cov}(x_{kp},x_{\ell p})\,a_{\ell p},\qquad
 a_{kp}=w_{kp}/W_p.
\]
\(x=y-\hat\delta\) 含估计 sky-plane 的参数、共同母版/PSF/响应、data-dependent rejection 和重采样相关误差；所以仅储存 \(W_p\) 并不足以无条件声称最优/独立，真实不确定度模型须另在 provenance 标记适用条件。

## 3. 真实几何、双序变换及单信号写盘

Coverage 的 `target_order=K_t` 是 HiPS **tile 阶数**，512 tile 的 \(\log_2(512)=9\)。当前实际叶阶 \(K_l=K_t+9\)，\(N_{\rm side}=2^{K_l}\)，每个 tile 512²=262144 叶；`AstroSphereTileView.leaf_order=K_l`，`parent_ipix` 是 tile 阶 NESTED parent。别把 `P2HipsInputInfo.max_leaf_order` 的旧字段名当像元阶，亦别混用 P1 FrameScience `signal.leaf_order`（它按真实叶阶声明）。P2 Writer 目标 `K_t` 的实际生产检查目前范围 0..20，同时验证 coverage 各帧阶一致；此范围不自动授权无边界分配或非法 32bit `nside`。

当前 integrate binary 按 tile FITS row-major 存，写器按 **HEALPix NESTED local** 将 \(i_{\rm FITS}\to i_{\rm local}\)；权威变换 `acsd::healpix::fits_index_to_nested_local(i,9,512)`；`fits_to_local` 长 262144，散射 `write_flux[local],write_cov[local]`，必要时 `write_varnum[local]`，每 tile 的唯一 `parent_ipix` 与输入 coverage tile ID 对齐。全部 262144 local 的映射应做一一对应、回环和极面/边界正反测试，不以画面肉眼接缝消失代替整数布局验证。缺 tile 与 tile 存在但数值 NaN 是不同的输入事实；无数据叶在唯一 signal 保留 NaN。

```text
write_mosaic(coverage, integrated, samples, sky_model, rejection):
  read p2_integrated.json and p2_coverage.json; verify input graph and uniform target_order
  validate integrated uncertainty_available/weight_basis/ivar_missing consistency
  bind input_manifest_hash, final sky_plane model_hash, rejection profile
  count response_distribution from every input frame's declared filter (including "")
  obs_filter = sole label when all same; else "mixed:<number of distinct labels>_passbands"
  stage = private run-specific revision directory; remove stale partial stage
  ps = aio_hips_product_begin(stage, nside=2^(target_order+9),
                              tile_width=512, dtype=F32,
                              product_flags=AIO_HIPS_PRODUCT_SIGNAL,
                              obs_filter=obs_filter)
  lut = fits_index_to_nested_local(0..512^2-1)
  parallel for tile in deterministic tile order with Runtime worker budget:
    read float64 integrated signal/support/wsum tile; verify offsets, sizes
    A = finite_positive(support) ? support*Omega_leaf : 0
    flux_sum[local] = float32(signal[FITS_index]*A)
    covered_area[local] = float32(A)
    optional diagnostic var_num = (A>0 and W>0) ? A^2/W : stated unavailable/invalid
  for tile in same deterministic order (serialized shared AIO writer):
    view = {parent_ipix,leaf_order,width=512,flux_sum,covered_area,valid_mask=null}
    aio_hips_write_signal_support_tile(ps,&view)
    if failed: abort and remove stage, record first failing tile/IO cause
  release tile reorder buffers; aio_hips_finalize(ps)
  declare real BUNIT/pixel_semantics; verify only signal product set and property/manifest
  publish staged signal revision + completed manifest (last); never expose incomplete as done
  re-open official signal/properties; write p2_final.json pointing to public paths
  record response_distribution, sky model identity, conditional uncertainty,
         actual weight basis, source hashes, provenance and publish status
```

这是当前 `module_adapters.cpp::p2_op_write` 的有名调用序而非旧 `stage2.cpp::main`。目前变换段 tile 可并行，AIO 写入、祖先累加/finalize 保持 tile 升序单句柄**串行**，否则会改变归约顺序和共享 scratch；返回错误时应优先确定性记录最小 tile 序首错，释放在途内存再删除 staging。当前实现先 stage、`aio_hips_verify_product_set`、`p2_publish_mosaic_tree` 换入并最后发布完成清单；独立消费者仍应校验 manifest/路径/hash 的一组 revision 一致，不以单个目录存在判断整个多文件事务成功。

## 4. 产品数据、字段和实际存在性

**生产实际路径**：`output_dir/signal/properties`、`output_dir/signal/Norder{K_t}/Dir*/Npix*.fits` 及必要 HiPS 上层汇总/覆盖元信息；`output_dir/p2_final.json`、完成 manifest；构建期间还有 `p2_integrated.json`、`p2_coverage.json`、`p2_sky_plane.bin`、`p2_samples.json`、`p2_rejection.json` 等内部算子证据。历史 `out_hips` 自带 `acsd-stage2` 单体 CLI、直接直写与 `signal/support/variance/ivar` 完成集只属于旧 `coverage/tools/stage2.cpp` 参考工具，不是新的 mosaic 命令形状。

| 字段及机器来源 | 保证或当前限定 | Export 真实依赖 |
|---|---|---|
| `signal/properties`: `hips_order`, `hips_tile_width`, `hips_frame`, `hips_ordering`, `BUNIT`, `obs_filter` | 必须匹配声明的 leaf/tile、ICRS/NESTED、面亮度 BUNIT；`obs_filter` 可为空且**键必须存在**；properties 不是物理响应曲线文件 | `p3_op_properties` 当前读取 BUNIT/order 等，**遗漏 obs_filter** (#79) |
| `p2_final.json`: `products`, `target_order`, `nside`, `n_tiles_written`, `bunit/units`, `uncertainty_available`, `uncertainty_unavailable_reason`, `weight_basis`, `ivar_product_missing_frames` | `products=[signal]`；unit/uncertainty 是可审计状态，不能由 absence of extra layers 猜全部方差不可用，也不能把诊断布尔当图像目录 | 需要 P3 把必要身份、单位和来源绑定进自己的 properties/最终 FITS |
| `p2_final.json:obs_filter/response_distribution` | 所有帧 label 多重集 `{label:count}`，空值记 `(undeclared)`；`mixed:n_passbands` 的 n 是**不同标签数**，不是源帧数；顺序不变 | P3 当前尚未读取/传播 (#79 已在真实 Export 复现) |
| `p2_final.json:provenance` `ACSD_INPUT_MANIFEST_HASH`, `ACSD_MODEL_HASH`, `ACSD_REJECT_PROFILE` | 来源需取实际冻结输入+最终生效 sky-plane（含 own）+真实参数/响应；`model_hash` 不等于文件路径/构建时间 | 需核验 hash、帧身份、input occurrence 与 response，不能只接受不透明字符串 |
| `phase2_audit` 与每节点状态 | n_used、秩、kappa_data/kappa_solve、实际参数自由度、组件/锚、异常；当前部分缺项可能仅 WARN，**不等于全部已审计** | 应保持 provenance 的能力与缺项，不以 summary 掩盖坏数据 |

FITS 的 `FILTER` 关键字只在能证明单一**非空名义标签**时可直接透传；混合或全 unknown 不选择第一帧 FILTER，也不把 `mixed:2_passbands` 伪称具体天文窄带。真正完整响应需要 `R_k(\lambda)`、QE、校准、位置依赖、实际贡献集合与模型身份；这是 [#14](https://github.com/fujiaze/Astro-CS-Database/issues/14) 的科学研究，不因为 label 分布有了就已完成。

## 5. `obs_filter`、unknown/mixed 与有效响应

输入覆盖顺序 **不属于物理身份**。定义 \(f_k\) 为每帧声明标签，\(\mathrm{counts}[f]=\#\{k:f_k=f\}\)。规范算法为：

```text
counts = sorted_count_multiset(frame.filter_passband for every frame)
if counts has exactly one key:
  obs_filter = that key (including "" for all-unknown)
else:
  obs_filter = "mixed:" + str(number_of_distinct_keys) + "_passbands"
response_distribution = { "(undeclared)" if key=="" else key: counts[key] }
```

含 `["","Halpha"]` 和 `["Halpha",""]` 均为 mixed:2；全 `["",""]` 为 unknown 空值，不允许凭“过滤后仅一种非空标签”就给 Halpha。业务运行可合并这些帧，但科学注记必须按来源覆盖。**#11 真实实验** `实验/issue11-passband-identity/evidence/REPORT.md` 的冻结时点报告 12/12 个 CLI 正例通过、同多重集 6 种排列元数据逐字一致、pre/post **185/185 signal tile DATA 精确一致**；其当时列出的 `signal/support/variance/ivar` 输出形态属于其冻结代码状态，不能与后续 D02 的“唯一 signal”混写；也**没有**证明 H-alpha 与 Red 的真实光谱及扩展源混合无偏。

固定接受集合且各 \(H_k\) 和输出积分被估计量确实可视为在同一点作用时，形式有效响应为
\[
 R_{\rm eff}(\lambda;\omega)=
 \frac{\sum_{k\in E(\omega)} w_k(\omega)R_k(\lambda)}
      {\sum_{k\in E(\omega)} w_k(\omega)}.
\]
若 PSF/Drizzle \(H_k\) 不同、背景与乘性校正从数据估计、rejection 改变集合，则这个表达只能是**受限近似**，无法仅从 `response_distribution`（只含帧计数）求出真正 \(R_{\rm eff}\)。响应真实状态必须在未来模型中声明 `single_known/unknown/mixed_nominal/physical_response_available` 等可区分语义，参数/误差/有效域分别记录；不能因为可写 mixed label 就自动宣称已有空间响应曲线。

## 6. P2 → P3 → FITS 的唯一责任与 #79 施工合同

[Issue #79](https://github.com/fujiaze/Astro-CS-Database/issues/79) 是**生产实测的元数据丢失**：#11 同一成功 Mosaic 产物上真实 `acsd export` exit 0、产生 `output_phase3.fits`，但 `p3_props.json` **不含** filter/response，FITS 头**没有** FILTER/RESP*；并非 Export 未执行，也不是 P5 物理增益已被证伪。最新 `module_adapters.cpp::p3_op_properties`（16055 起）只经 unit guard + `p3_sampler_open_ex` 读取 BUNIT/order；`p3_op_writer`（16614 起）用 resample、WCS 和运行上下文创建 FITS，未从 p2_final 解析 response。这是确定的 API/数据交接缺口。

**选定目标字段传输（设计冻结；机器 schema/实现尚 NOT_RUN）**：

| 边界 | 读入 → 生产/持久化 → 消费 | 必须的失败语义 |
|---|---|---|
| P2 Writer | 将 `obs_filter`、规范化 `response_distribution` 与 `response_identity[]`（逐 occurrence 的 frame_id, 原始 FITS/content hash, filter/QE/曲线 hash, photometric application ID, 实际有资格支持域）绑定到 `p2_final.json` 和可独立哈希的 provenance manifest，记录 `physical_response_status`，与本次 signal revision 绑定 | label 可缺物理曲线但须记 `unknown`；response model 不可求值不能标 `physical_response_available` |
| P3 properties | 由 `p3_op_properties` **先验证 signal 产品完成指针和 `p2_final.json` 的同一 root/revision、哈希/几何/units/响应身份**，再把 `obs_filter`、分布、response identity refs 和 `response_provenance_sha256` 按原名写入 `p3_props.json`，并保留原始 `BUNIT` 与 canonical 描述 | P2 已声明而 P3 不消费/值不一致 = DATA_CONFLICT；不完整/坏 hash = DATA_CORRUPT；独立外部合法 HiPS 缺 ACSD 附加字段时，明确 `unknown_origin` 而不猜 FILTER |
| P3 resample/writer | 将上游身份字段不变透传 `p3_resampled.json`/运行 manifest；FITS PRIMARY 写 `BUNIT`、WCS、有效 FILTER (只限 sole nonempty label)、`ACSDRESP`（`SINGLE`/`UNKNOWN`/`MIXED`）、`RSPHASH`（完整 64hex 的 sidecar provenance SHA256）。mixed 时**不**写任一具体 FILTER；all unknown 也不写 FILTER；响应完整原表在随成果交付的 provenance JSON（由 FITS 头 RSPHASH 精确定位/核对） | 若声称响应完整但 sidecar 遗失，不能只输出 FITS 并标 VERIFIED；FITS 外部单文件仍必须通过 ACSDRESP 区分未知与混合，即使外部不能获取完整 sidecar |
| P3 verify + 独立 reader | 重开真实 FITS 用 CFITSIO/astropy 解析 `FILTER`（若有）/ `ACSDRESP`/`RSPHASH`，对本次 P2 原信号与 provenance 重新核验，检查输出 mode、BUNIT、WCS/crop 与像素数值未变 | 单一 Halpha、全空、mixed2/mixed3、损坏分布/坏 hash/另一个 run 伪替换各有具名结果；科学图像 DATA 允许只在重采样/输出精度声明的误差内改变 |

这里 `ACSDRESP`、`RSPHASH` 是**目标 FITS 8 字符关键字**，不是声称现有 C++/Schema 已支持；full hash 合法 64hex FITS 字符串必须按 FITS card 长度/读取库实际验证。超出 FITS 单卡的明细用外部 JSON（自身哈希固定）和可选合规长字符串/附表，不用截断哈希伪称完整。必要时区分导出标准天文产品与可携完整私有 provenance 的发布包；单文件 FITS 仍应具备最基本的单/混/未知状态，真实响应 SED/position 信息通过外部模型读取。**不因仅有名义 metadata 就生成 `R_eff` 真值**。

## 7. 原子发布、路径及错误/取消状态

正式 Reader 只消费具有一致完成 manifest/revision 的 `signal/`；staging 里的 tiles 即使 FITS 有效也不是完成产品。Writer 的输入必须完成 `p2_integrated.json`、coverage 阶数校验和元数据协议，含 `uncertainty_available` 必须为显式 bool；若真则 `weight_basis` 必须是可解释的逐样本 ivar/校正 variance，缺 ivar 或 snr_chain_used 不能谎称完整精度。旧 `weight_mode` 数值键不应作为正式输出状态选择项。模型和输入 hash 在 stage 验证之前固定；旧 model hash（不含 own）不可沿用。

`aio_hips_product_begin → write_signal_support_tile → finalize → verify_product_set → publish_mosaic_tree` 期间错误逐级收敛：非法几何/混阶/输入缺失/截断，负或非有限候选权重，无法序转换或数组长度不等，坏模型、坏 response/provenance、临时目录不可删除、磁盘写满、AIO/CFITSIO IO 错误、cancel 和完成 manifest/hash 不符。每种带 `frame/run/tile/phase` 和首错；磁盘满按真实故障窗口上行，不能随普通失败洗掉；失败清理 staging，不重新标记旧 partial 为完成。已发布 revision 的合法读者应始终看到旧完整或新完整（或显式 pending），不把 `rename` 一个文件说成天然多文件原子事务。

**真正发布动作只包括一份 signal**，且 `p2_final.json.products` 和磁盘事实双向相等。当前 writer 为写线程串行、转换阶段经 Runtime worker 预算并行；`aio_hips_finalize` 会递归汇总父阶而非在 Norder 叶单目录就结束。构建临时缓存 `write_flux/write_cov` 每 tile 各 262144×4 B，若 `uncertainty_available` 另 `write_varnum`；一次预分配 T 个 tile，峰值约 \(T\cdot262144\cdot4\cdot(2+\mathbf1_{\rm var})\) B 再加 LUT/输入片段和 AIO scratch，**不是**历史 micro-chunk 的 \(O({\rm depth}\times{\rm chunk})\) 恒定内存。当前 Tile 读/序转换可按 T 并行，但大 T 的预算、AIO 背压与 523 tile 等量级工作集必须经实际资源测量/分段处理，不能仅因“CPU并行”就认定不会 OOM。

## 8. 独立核验与旧长卷技术信息的正确继承

| 由原长卷保留的实验思想 | 现行唯一正反 oracle | 已具备证据 / 缺口 |
|---|---|---|
| ALG-001：帧/产品/哈希链 | 外部扫描 output 只含 Signal，完成 manifest、signal/properties、tile/header 与 p2_final 的引用完整；故意删一文件/换 frame 或 response，必须明确失败 | 当前 writer 有 stage+verify+publish 生产分支；D5未重新测试 |
| ALG-002：tile 次序/几何/并行 | 所有 512² FITS index 与 nested index 双向互逆；K_t=0/2/4，跨 face/polar/RA wrap、不同 worker 产生相同像元和声明 | 旧 `stage2.cpp` 路径测试/行号不能当 `p2_op_write` 的 E2E |
| ALG-003：权重/六态/排异/相关 | 合成两/三帧精确加权 \(\sum w x/\sum w\)、support=max accepted，独立 Python/高精度 oracle；零权、坏权、全排异、不存在精度与相同权重不同相关协方差分别对照 | 新 D5 未跑；原 `Phase2IvarWiring` 是旧工具测试，不能代替最新适配器 |
| ALG-004：数值编码与 Writer | 比对 integrated `signal/support/wsum` 双精度与 final tile FP32（包含 \(Q=\mathrm{signal}\times A\)、\(B=Q/A\) 的舍入误差）；缺覆盖 NaN、面积有限性/溢出/无效三态 | 正式 E2E 未由 D5 执行 |
| #11：标签排列与内容正交 | `["",Halpha]`、`[Halpha,""]`、全空、两/三种通带；比较属性/manifest/FITS 与 tile DATA | #11 当次 12/12 CLI、185/185 数组通过；这验证**当次 label-only**，非不同真实响应的天体物理 |
| #79：P2→P3→外部读取 | 真实 Halpha/unknown/mixed、不同原始曲线 SHA、坏 provenance、已发布 P2→Export 后外部 FITS header 与响应 JSON hash；信号数值除本来 Export 重采样外不改 | #79 的真实失败已观察；D5 文档目标，代码和测试 NOT_RUN |
| #80/#76：背景与乘性科学质量 | HST / 真实 M42 负阳真值、同物理响应与跨响应、已知星云结构/星点流量/低频功率、理论方差与 Monte Carlo、E2 目检 | 研究 OPEN，不能因接缝 RMS 缩小而判定通过 |

原文附录 `stage2.cpp` 的 CLI 退出码 0/1/2/3/4/5/6/7、`legacy_allow_weight_fallback`、tile probe \(O(TN_f)\)、ACR/CPU reference/cuda bridge 和 `AIO_HIPS_PRODUCT_SIGNAL|SUPPORT` 只保留为**退役工具实现/旧测试背景**；它们不应指导今天的 `p2_op_write` 返回码、输出数量或资源配置。当前七节点实际 `Result<void>` 与 CLI 统一 exit 映射是权威，工具旧 rc 不等于生产 Result.ErrorDomain。

**参考与标准：** [IVOA HiPS 1.0](https://www.ivoa.net/documents/HiPS/)；Fernique et al. (2015), A&A 578 A114；Górski et al. (2005), ApJ 622 759（HEALPix）；[FITS Standard](https://fits.gsfc.nasa.gov/fits_standard.html) 与 [CFITSIO](https://heasarc.gsfc.nasa.gov/fitsio/)；Fruchter & Hook (2002), PASP 114 144（Drizzle）；Zackay & Ofek (2017) I/II；Bertin et al., SWarp (2002) 以及 Gruen et al. (2014)（背景/叠加对照）。原文使用的 SWarp (GPL) 是算法参考，不自动授权代码复制。上述文献链接和公式支持设计讨论，**D5 未逐篇重新精读全文**。
