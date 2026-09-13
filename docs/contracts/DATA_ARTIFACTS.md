# DATA-001 DataArtifact 注册表与 weight 歧义映射

> 权威: docs/contracts/DATA_SEMANTICS.md（语义）+ 本表（artifact schema 登记 + 歧义映射）
> 生成: V6 重构 DATA-001

## 1. DataArtifact schema 清单

| schema_id | 内容 | scalar | shape/axis | unit | coordinate | invalid | ownership | serialization |
|---|---|---|---|---|---|---|---|---|
| DATA-IMG-RAW-001 | raw 亮场 | f32/f64/u16 | [H,W], y,x | ADU | pixel(0-based) | NaN=invalid | unique→borrowed | FITS/XISF |
| DATA-IMG-CAL-001 | calibrated 亮场 | f32/f64 | [H,W], y,x | ADU(e⁻ 可选) | pixel | NaN=invalid; 负值保留 | unique | FITS |
| DATA-IMG-VAR-001 | variance | f32/f64 | [H,W] | ADU² | pixel | 0=无覆盖; NaN/负=损坏 | unique | FITS |
| DATA-IMG-IVAR-001 | inverse variance | f32/f64 | [H,W] | ADU⁻² | pixel | ivar=0 显式不可用 | unique | FITS |
| DATA-IMG-WEIGHT-001 | quality weight | f32 | [H,W] | 无量纲[0,1] | pixel | 0=不合格 | unique | FITS |
| DATA-IMG-SUPPORT-001 | support | f32/u8 | [H,W] | [0,1] | pixel | 0=无覆盖 | unique | FITS/HiPS |
| DATA-IMG-MASK-001 | bad-pixel mask | u8 | [H,W] | 位掩码 | pixel | 1=坏点 | unique | FITS |
| DATA-WCS-001 | WCS 描述 | struct | n/a | deg/px | ICRS | 无解=显式错误 | shared | header/JSON |
| DATA-CAT-PSF-001 | PSF catalog | struct[] | [N] | px,deg,ADU | ICRS+pixel | 失败不输出伪有效 | unique | FITS/CSV |
| DATA-CAT-PHOT-001 | photometry catalog | struct[] | [N] | e⁻,mag | ICRS | 饱和拒=rejected_quality | unique | FITS/CSV |
| DATA-HIPS-SIGNAL-001 | HiPS signal 数据集 | f32/f64 | HEALPix NESTED | 面亮度 | ICRS | NaN/support=0 | shared/persisted | HiPS |
| DATA-HIPS-VAR-001 | HiPS variance | f32/f64 | HEALPix | ADU² | ICRS | 0=无覆盖 | persisted | HiPS |
| DATA-HIPS-IVAR-001 | HiPS ivar | f32/f64 | HEALPix | ADU⁻² | ICRS | ivar=0 | persisted | HiPS |
| DATA-UPM-MODEL-001 | UPM 加性模型 | double[] | [n_frame×n_cell] | ADU | control cell | NO_DATA 显式 | persisted | UPM file |
| DATA-UPM-CONTROL-UNC-001 | control variance/ivar | f64 | [n_control] | ADU²/ADU⁻² | control cell | ivar≤0→rc=2 | unique | JSON |
| DATA-REJ-MAP-001 | rejection map | u8 | [H,W] | 位掩码 | pixel | reason code 每样本 | persisted | FITS |
| DATA-FRAME-ID-001 | frame identity | uint64 | scalar | 无量纲 | 科学 payload 派生 | 重复拒绝 | shared | JSON/manifest |
| DATA-P3-FITS-001 | 平面 FITS | f32/f64 | [W_out,H_out] | 面亮度(禁默认 Jy/beam) | TAN/ICRS | NaN+coverage | persisted | FITS |
| DATA-GAIA-001 | Gaia XPSD 星表行(C ABI 输出) | f64/i32/u8[] | [out_count]; 光谱 [out_count×spec_n] | deg,mag,W·m⁻²·nm⁻¹,nm | ICRS J2000 | out_match_idx=−1 未匹配; out_count=0 空结果合法; DR3 下 BP/RP=0 sentinel; 无光谱 flux_min/mul=0 | caller free(顶层 malloc) | in-memory(不落盘, §8.3) |
| DATA-COV-001 | Phase2 coverage 联合 MOC(P2CoverageResult) | u64/u64 | union_cells [K](P2MocCell: order+ipix); K=n_union_cells 标量; inputs [n_inputs] | 无量纲(order/ipix) | HEALPix NESTED 父单元(ipix<12·4^order, 去重升序) | K=0 空结果合法(rc=0); 两阶段协议第一次调用不写 union_cells/inputs | caller free(调用方分配, coverage.h:26-48) | in-memory(不落盘) |
| DATA-UNC-001 | Phase2/Phase3 不确定度产品合同容器(DATA_SEMANTICS §30, 目标态) | 容器(无标量) | n/a | n/a | 跨域(见各子 schema) | 实现不得先行; unavailable 显式登记模式 | owner(DATA-001 冻结) | DATA_SEMANTICS.md §30 |
| DATA-P2-VAR-001 | Phase2 马赛克 variance/ivar 子产品(目标态) | f32/f64 | HEALPix NESTED 512 tile | ADU²/ADU⁻² | ICRS | 无有效样本=NaN(signal=NaN 同态); 非有限合成=NaN; 禁 0/±Inf 伪装 | persisted(variance/,ivar/ 目录) | HiPS(AIO_HIPS_PRODUCT_VARIANCE=8/IVAR=16) |
| DATA-P2-REJ-001 | Phase2 rejection 产品 nused/nrej(目标态, 诊断统计平面) | int32 | HEALPix NESTED 512 tile | 无量纲计数 | ICRS | 无覆盖=0(禁 −1 哨兵); 逐帧 reason 级非目标 | persisted(nused/,nrej/ 目录; AIO 位 64/32 冻结分配) | HiPS(不入 exchange science planes 枚举) |
| DATA-P2-PROV-001 | Phase2 provenance 键组(目标态) | 64hex/uint/string/bool | 标量×5 | 无量纲 | 无(元数据) | uncertainty_available=false 显式登记非失败; 禁缺键/占位 | persisted(properties+manifest.json 双写) | HiPS properties+JSON |
| DATA-P3-UNC-001 | Phase3 重采样 uncertainty 传播产品(目标态) | f32/f64 | [W_out,H_out] 行主序 | ADU²/ADU⁻²(BUNIT 派生) | TAN/ICRS(FITS-WCS) | 无覆盖=NaN(C=0); NaN 传播=C=1; 负/Inf=损坏显式错误; unavailable=无 HDU+manifest 标记 | persisted(单 FITS 文件 VARIANCE/IVAR 扩展 HDU) | FITS(EXTNAME=VARIANCE/IVAR, DATASUM 逐 HDU) |
| DATA-HIPS-001 | HiPS 产品输入面(properties + signal tile 读路径; 正文=SCI-P3-001 §9a-1 + DATA_SEMANTICS §29.1, tile 读路径=§3 冻结) | f32(tile) + 文本(properties) | 512×512/leaf(W=hips_tile_width, leaf=HEALPix cell @hips_order) | 面亮度(BUNIT 透传, 缺省 ADU; 禁默认 Jy/beam) | HEALPix NESTED(frame=ICRS; tile 内 FITS local 映射=§3) | NaN=传播语义非 invalid; 缺 tile=无覆盖非错误; properties 必需键非法→显式 P3_RS_PARAM | shared(只读共享; 产品归生产方 Phase1/Phase2) | HiPS 目录树(properties + FITS tiles) |
| DATA-TILE-001 | 单个 HiPS leaf tile 科学面(W×W FITS float tile; 正文=SCI-P3-001 §9a-1/-8 + DATA_SEMANTICS §3) | f32 | [W,W] FITS local(W=512, leaf=NESTED 18 bit) | 面亮度(surface brightness; BUNIT 透传, 缺省 ADU) | HEALPix NESTED leaf + tile 内 fits_index=(511−x)·512+y(§3 CDS oracle 冻结) | tile 内 NaN=传播非 invalid; 缺 tile=无覆盖非错误; 非 float/多通道/JPEG-PNG/int+BLANK=显式拒绝 | shared(sampler 只读缓存, 禁改写) | FITS tile(HiPS 目录内) |

### 1.1 登记行权威锚点（CI-DATA-REG-001，2026-09-12）

上表末两行 `DATA-HIPS-001` / `DATA-TILE-001` 是**既存** ID 的登记补齐，
**不是新语义**：两 ID 早已在 DATA_SEMANTICS §29.5（"DATA-HIPS-001/
DATA-TILE-001（HiPS properties/tile 输入面）"）、生产 descriptor
（lib/core/src/module_adapters.cpp:498-499 与 :459/:498）、
runtime/pipeline/module_ports.registry.json:246/:277、registry 端口表
（docs/modules/registry/astrocs.phase3.resample.md、…resample2.md、
…properties.md）、tests/unit/core_pipeline_test.cpp:66-67 在用，并在
docs/traceability/TRACEABILITY_MATRIX.csv:25/:31 登记为 `VERIFIED`。

本登记只把**既存冻结正文**映射为登记行，未新增/修改任何公式、单位、
坐标系、精度或 invalid 规则。逐行正文锚点：

- `DATA-HIPS-001`（HiPS 产品输入面）: SCI-P3-001（docs/science/
  PHASE3_HIPS_TO_FITS.md）§4 输入有效域（properties 必需键
  `hips_order/hips_tile_width/hips_frame/dataproduct_type=image`；NESTED
  唯一；仅 float FITS tiles）、§9a-1（tile=HEALPix cell @hips_order 的
  W×W FITS float tile）、§9a-8/-9（tile 值=面亮度；缺 tile=无覆盖）、
  §8（tile 内 NaN 传播）；DATA_SEMANTICS §2（NESTED/leaf_order）、
  §3（FITS tile local-pixel 映射，V5/V11 冻结）、§4（signal/support/
  invalid）、§29.1（hips_dir 严格校验 + HiPS tile 读路径行）。单位口径
  =§29.2/§29.3（采样值=面亮度，BUNIT 透传缺省 `ADU`，绝不默认 Jy/beam）。
- `DATA-TILE-001`（单 leaf tile 面）: 同上 §3 冻结映射（tile 内
  `fits_index=(511−x)·512+y`）与 SCI-P3-001 §9a-1/-8；descriptor 单位
  `UnitId::SURFACE_BRIGHTNESS`、坐标 `CoordinateFrame::HEALPIX`
  （module_adapters.cpp:499）；"tile 读路径权威=DATA_SEMANTICS §3"
  （docs/modules/phase3_rsmp.md:75）。

**未决偏差（登记 finding，修复落本任务写域外）**: `DATA-HIPS-001` 的
coordinate 在端口词汇面存在两个值——`CoordinateFrame::PIXEL`
（module_adapters.cpp:498、docs/modules/registry/astrocs.phase3.resample.md:22）
与 `CoordinateFrame::HEALPIX`（module_adapters.cpp:521/:560、
…resample2.md:71、…properties.md:22）。本表按冻结正文（§29.1/§3）登记
HEALPix NESTED；PIXEL 一侧属端口词汇漂移（§29.5 已声明端口表"不得
反向作为冻结依据"），修复归 descriptor/registry 域（P3-RSMP-INT /
DATA-001），不在本任务写域。


## 2. weight/value/scale/sigma/snr 歧义映射（DATA-001 登记）

控制包 06 要求：对现有 `weight/scale/sigma/value/snr` 字段逐个映射或登记 ambiguity；
未消除 ambiguity 不得进 G2 consumer API。

| 字段名 | 出现位置 | 语义 | DATA ID | 歧义状态 |
|---|---|---|---|---|
| `weights` (integrate) | lib/phase2/include/astro/phase2/integrate.h | 候选栈数值权重 = support×SNR² 或等权(1.0) | DATA-IMG-WEIGHT-001 数值权重 | 已消除(与 UPM 权重分离命名) |
| `weights` (rejection) | lib/phase2/include/astro/phase2/rejection.h | 随样本携带到候选栈的数值权重 | 同上 | 已消除 |
| `upm.robust_control_weight` | upm.cpp | UPM 控制点权重 = quality×geom×control_ivar | DATA-UPM-CONTROL-UNC-001 | 已消除(禁止与 integration weight 混名) |
| `support` | integrate/upm/sampler | 覆盖支撑 [0,1] | DATA-IMG-SUPPORT-001 | 明确 |
| `scale_deg_per_px` | p3_session | 输出像元角尺度 | DATA-P3-FITS-001 s_out | 明确(单位 deg/px) |
| `sigma` | master_generator/rejection | MAD 转 σ 系数 1.4826 / 拒绝阈值倍数 | SCI-NOISE/SCI-REJ | 明确(无量纲倍数) |
| `snr` | CW/sampler | 区域级 SNR 权重因子 snr_v² | SCI-CW-001 | 明确(与 ivar 语义分离) |
| `value` (integrate) | integrate.h | 候选样本值 | DATA-IMG-CAL-001 标度 | 明确 |
| `quality` | sampler | 帧/星点质量位掩码 | SCI-CW-001 | 明确 |
| `k_corr` | sampler.cpp:672 | Drizzle 相关校正 1.4 | SCI-UPM-WEIGHT-001 | 明确 |

结论：`weight` 在 integrate 与 UPM 两处语义已显式分离命名（`stack.*.v1` vs
`upm.robust_control_weight.v1`），无未消除歧义；`scale/sigma/snr/value/quality` 均有
明确 DATA/SCI 归属。G2 consumer API 可安全引用。

## 3. 机器校验

- `tools/check_data_artifacts.py`（DATA-001 新增）：校验本表 schema_id 唯一、
  DATA-SEMANTICS.md 中声明的 DATA-* ID 全部在本表登记、无重复。
