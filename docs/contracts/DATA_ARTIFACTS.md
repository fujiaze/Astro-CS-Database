# DATA-001 DataArtifact 注册表与 weight 歧义映射

> 上游：ASTROCS_DESIGN.md §10（I/O 与原子产品）

> 权威: docs/contracts/DATA_SEMANTICS.md（语义）+ 本表（artifact schema 登记 + 歧义映射）
> 生成: V6 重构 DATA-001

## 1. DataArtifact schema 清单

| schema_id | 内容 | scalar | shape/axis | unit | coordinate | invalid | ownership | serialization |
|---|---|---|---|---|---|---|---|---|
| DATA-IMG-RAW-001 | raw 亮场 | f32/f64/u16 | [H,W], y,x | ADU | pixel(0-based) | NaN=invalid | unique→borrowed | FITS/XISF |
| DATA-IMG-CAL-001 | calibrated 亮场 | f32/f64 | [H,W], y,x | ADU(e⁻ 可选) | pixel | NaN=invalid; 负值保留 | unique | FITS |
| DATA-IMG-VAR-001 | variance（逐帧/逐帧内平面，**帧内块**） | f32/f64 | [H,W] | **量纲 `ADU^2`（像素域，无 sr 幂）；标度 = 与同帧 `data` 面同一标度**：`calibrated_adu` ⇒ ADU²；`photo_scaled_adu` ⇒ α²·ADU²（α = frame photscal，**逐帧量**，见 `docs/standards/NUMERIC_STANDARD.md` 标度词表） | pixel | **无方差信息 = 0（显式不可用，§4a）**；NaN/负 = 损坏（写侧 rc=−6 硬失败） | unique | FITS |
| DATA-IMG-IVAR-001 | inverse variance（帧内块） | f32/f64 | [H,W] | **量纲 `ADU^-2`（像素域）；标度与同帧 variance 严格互倒**（`ivar = 1/variance`，有限域） | pixel | **ivar=0 = 显式不可用**（禁 `1/0→Inf`）；NaN/负 = 损坏 | unique | FITS |
| DATA-IMG-WEIGHT-001 | quality weight | f32 | [H,W] | 无量纲[0,1] | pixel | 0=不合格 | unique | FITS |
| DATA-IMG-SUPPORT-001 | support | f32/u8 | [H,W] | [0,1] | pixel | 0=无覆盖 | unique | FITS/HiPS |
| DATA-IMG-MASK-001 | bad-pixel mask | u8 | [H,W] | 位掩码 | pixel | 1=坏点 | unique | FITS |
| DATA-WCS-001 | WCS 描述 | struct | n/a | deg/px | ICRS | 无解=显式错误 | shared | header/JSON |
| DATA-CAT-PSF-001 | PSF catalog | struct[] | [N] | px,deg,ADU | ICRS+pixel | 失败不输出伪有效 | unique | FITS/CSV |
| DATA-CAT-PHOT-001 | photometry catalog | struct[] | [N] | **通量标度 = `calibrated_adu`（ADU）；`e⁻` 只在调用方另行给出 gain 换算后才成立，换算责任方 = 消费方**（本层不建模 gain：`docs/science/CALIBRATION.md` §1 非目标、§9a「gain 不在本层建模」）；星等无量纲 | ICRS | 饱和拒=rejected_quality | unique | FITS/CSV |
| DATA-HIPS-SIGNAL-001 | HiPS signal 数据集 | f32/f64 | HEALPix NESTED | 面亮度 | ICRS | NaN/support=0 | shared/persisted | HiPS |
| DATA-HIPS-VAR-001 | HiPS variance 子产品 | f32/f64 | HEALPix NESTED tile | **`ADU^2/sr^2`（面亮度方差 = `var_num_sum/covered_area²`；立体角幂不可省，见 `docs/standards/NUMERIC_STANDARD.md` 面亮度标度律）** | ICRS | **无覆盖（covered_area≤0）= NaN（与 signal 同态）**；有覆盖但无方差信息（covered_area>0 ∧ var_num_sum≤0）= **0**（显式不可用）；负 = 损坏。编码权威 = `DATA_SEMANTICS` §11.2/§12.2/§4a | persisted | HiPS |
| DATA-HIPS-IVAR-001 | HiPS ivar 子产品 | f32/f64 | HEALPix NESTED tile | **`sr^2/ADU^2`（= 1/variance，有限域互为倒数）** | ICRS | **无覆盖 = NaN（同 signal）**；有覆盖但无方差信息 = **0**（禁 `1/0→Inf`）；负 = 损坏。编码权威 = `DATA_SEMANTICS` §11.2/§12.2/§4a | persisted | HiPS |
| DATA-UPM-MODEL-001 | UPM 加性模型 | double[] | [n_frame×n_cell] | ADU | control cell | NO_DATA 显式 | persisted | UPM file |
| DATA-UPM-CONTROL-UNC-001 | control variance/ivar | f64 | [n_control] | **`ADU^2` / `ADU^-2`（像素域，无 sr 幂）；标度 = 与所消费的 Phase1 帧面同一标度**（帧已施加测光时随 `photo_scaled_adu` 一并缩放，`Var ∝ α²`） | control cell | ivar≤0→rc=2 | unique | JSON |
| DATA-REJ-MAP-001 | rejection map | u8 | [H,W] | 位掩码 | pixel | reason code 每样本 | persisted | FITS |
| DATA-FRAME-ID-001 | frame identity | uint64 | scalar | 无量纲 | 科学 payload 派生 | 重复拒绝 | shared | JSON/manifest |
| DATA-P3-FITS-001 | 平面 FITS | f32/f64 | [W_out,H_out] | 面亮度(禁默认 Jy/beam) | TAN/ICRS | NaN+coverage | persisted | FITS |
| DATA-P1-PHOTPROV-001 | Phase1 测光 provenance sidecar(p1_phot.json; B2-A14 关闭伪造 PHOTAPPL) | 无标量(标签+比例) | n/a | 无量纲 | n/a | 缺文件=未应用测光(允许显式 ADU 降级); schema≠DATA-P1-PHOTPROV-001 / photscal 非有限或≤0 → drizzle DATA 拒绝; 禁硬编码 PHOTAPPL=1。**DET-001(D5)**：该「缺文件」分支只允许是**显式**状态——CLI 链上 phot→drz 有 typed 依赖边（phot 输出端口 p1_phot → drz 输入端口 p1_phot, artifact:p1_phot），调度器保证 phot 落盘后才执行 drz，禁止再用并发文件约定判定存在性（修复前实测 drz 早启 0.27–0.33 s 的 4 次全部误记 absent）。**逐帧判决（FAILSEM-01）**：`frames[]` 逐帧记 `status ∈ {ok, fail}`，`status=fail` 必须带 `error_domain`/`error_status`（稳定错误码）/\`error\` 且 `photometry_applied=false`、不产出 `photoapplied_<base>`、不写 `degraded_reason`（失败≠降级）；`photometry_applied` 是**组级摘要**（至少一帧已施加，`pixel_scaling` ∈ applied/partial/none），逐帧真相只在 `frames[]`；不变量 `n_frames_ok + n_frames_failed == n_frames`、`len(photscales) == n_frames_applied`、`failed_frames` 与 `frames[]` 一致（门禁 `CHK-PROVENANCE-CONSISTENCY` P3b/P3b-2） | unique(p1_op_photometry 原子写) | JSON |
| DATA-GAIA-001 | Gaia XPSD 星表行(C ABI 输出) | f64/i32/u8[] | [out_count]; 光谱 [out_count×spec_n] | deg,mag,W·m⁻²·nm⁻¹,nm | ICRS J2000 | out_match_idx=−1 未匹配; out_count=0 空结果合法; DR3 下 BP/RP=0 sentinel; 无光谱 flux_min/mul=0 | caller free(顶层 malloc) | in-memory(不落盘, §8.3) |
| DATA-COV-001 | Phase2 coverage 联合 MOC(P2CoverageResult) | u64/u64 | union_cells [K](P2MocCell: order+ipix); K=n_union_cells 标量; inputs [n_inputs] | 无量纲(order/ipix) | HEALPix NESTED 父单元(ipix<12·4^order, 去重升序) | K=0 空结果合法(rc=0); 两阶段协议第一次调用不写 union_cells/inputs | caller free(调用方分配, coverage.h:27-48) | in-memory(不落盘) |
| DATA-UNC-001 | Phase2/Phase3 不确定度产品合同容器(DATA_SEMANTICS §30, 目标态) | 容器(无标量) | n/a | n/a | 跨域(见各子 schema) | 实现不得先行; unavailable 显式登记模式 | owner(DATA-001 冻结) | DATA_SEMANTICS.md §30 |
| DATA-P2-VAR-001 | Phase2 马赛克 variance/ivar 子产品(目标态) | f32/f64 | HEALPix NESTED 512 tile | **`ADU^2/sr^2` / `sr^2/ADU^2`**（面亮度方差/逆方差；立体角幂不可省，见 `docs/standards/NUMERIC_STANDARD.md` 面亮度标度律） | ICRS | 无有效样本=NaN(signal=NaN 同态); 非有限合成=NaN; 禁 0/±Inf 伪装 | persisted(variance/,ivar/ 目录) | HiPS(AIO_HIPS_PRODUCT_VARIANCE=8/IVAR=16) |
| DATA-P2-REJ-001 | Phase2 rejection 产品 nused/nrej + 逐样本接受掩码 sample_mask(目标态, 诊断统计平面 + integrate 原始样本索引资格载体) | int32 + u8 | HEALPix NESTED 512 tile + 逐 tile [depth×tile_span] | 无量纲计数 + 0/1 接受位 | ICRS | 无覆盖=0(禁 −1 哨兵); sample_mask 缺失/offset 错位/frame_slots 不符/字节∉{0,1} → integrate fail-closed(禁回退像素级 accepted); 逐帧 reason 级非目标 | persisted(nused/,nrej/ 目录; AIO 位 64/32 冻结分配; files.sample_mask=p2_rejection_sample_mask.bin) | HiPS(不入 exchange science planes 枚举) |
| DATA-P2-PROV-001 | Phase2 provenance 键组(目标态) | 64hex/uint/string/bool | 标量×5 | 无量纲 | 无(元数据) | uncertainty_available=false 显式登记非失败; 禁缺键/占位 | persisted(properties+manifest.json 双写) | HiPS properties+JSON |
| DATA-P3-UNC-001 | Phase3 重采样 uncertainty 传播产品(目标态) | f32/f64 | [W_out,H_out] 行主序 | **`ADU^2/sr^2` / `sr^2/ADU^2`**（BUNIT 派生；立体角幂不可省） | TAN/ICRS(FITS-WCS) | 无覆盖=NaN(C=0); NaN 传播=C=1; 负/Inf=损坏显式错误; unavailable=无 HDU+manifest 标记 | persisted(单 FITS 文件 VARIANCE/IVAR 扩展 HDU) | FITS(EXTNAME=VARIANCE/IVAR, DATASUM 逐 HDU) |
| DATA-P3-REJ-001 | Phase3 重采样**强制剔除计数**诊断统计平面（§30.7；正本 = DATA-002 §2a 规则 3 `count_field=n_rejected_nonfinite`） | int32 | W×H 一平面（独立载体 `p3_rejection.bin`） | 无量纲计数 | 输出平面像素（行主序，与 signal 同几何） | 无覆盖=0（**0 即「无」**，禁 −1 哨兵）；**字段缺失 ≠ 全 0**（缺 `diagnostic_planes.n_rejected_nonfinite` 的产品不得声称满足 §2a 规则 3，判据具名 `COUNT_FIELD_MISSING`） | unique（p3_op_writer 原子写） | 二进制（**不进** exchange science planes 枚举；manifest `diagnostic_planes.n_rejected_nonfinite` + `n_rejected_nonfinite_total` 声明；判据 `eng/tools/quality/check_p3_rejection_count.py`） |
| DATA-HIPS-001 | HiPS 产品输入面(properties + signal tile 读路径; 正文=SCI-P3-001 §9a-1 + DATA_SEMANTICS §29.1, tile 读路径=§3 冻结) | f32(tile) + 文本(properties) | 512×512/leaf(W=hips_tile_width, leaf=HEALPix cell @hips_order) | 面亮度(BUNIT 透传, 缺省 ADU; 禁默认 Jy/beam) | HEALPix NESTED(frame=ICRS; tile 内 FITS local 映射=§3) | NaN=传播语义非 invalid; 缺 tile=无覆盖非错误; properties 必需键非法→显式 P3_RS_PARAM | shared(只读共享; 产品归生产方 Phase1/Phase2) | HiPS 目录树(properties + FITS tiles) |
| DATA-TILE-001 | 单个 HiPS leaf tile 科学面(W×W FITS float tile; 正文=SCI-P3-001 §9a-1/-8 + DATA_SEMANTICS §3) | f32 | [W,W] FITS local(W=512, leaf=NESTED 18 bit) | 面亮度(surface brightness; BUNIT 透传, 缺省 ADU) | HEALPix NESTED leaf + tile 内 fits_index=(511−x)·512+y(§3 CDS oracle 冻结) | tile 内 NaN=传播非 invalid; 缺 tile=无覆盖非错误; 非 float/多通道/JPEG-PNG/int+BLANK=显式拒绝 | shared(sampler 只读缓存, 禁改写) | FITS tile(HiPS 目录内) |

### 1.2 统一对象合同登记（DATA-001，UNIFIED_MODEL §2，canonical 在 eng/contracts/schemas/）

本表第二节：UNIFIED_MODEL §2 的 **13** 个对象各自在 `eng/contracts/schemas/unified/<对象名>.schema.json` 有唯一 canonical 合同（原 14 个；`psfsw_robust_weight` 已于 2026-09-20 按负责人裁决 B 真删，14→13，见 `docs/design/UNIFIED_MODEL.md` §2 与 GAP_AUDIT §4.5 C01）（`$id = https://astrocs.local/schemas/unified/<对象名>/v1`）。对象身份 / 单位（含 BUNIT 语义）/ 无效值与缺失表示 / 精度 / 可否作权重一律以该 canonical schema 为准；本表只登记它们在 DataArtifact 面的 scalar/shape/unit/coordinate/invalid/ownership/serialization 六列。
上位锚：`docs/design/UNIFIED_MODEL.md` §2；`ASTROCS_DESIGN.md` §2；`ENGINEERING_SPEC.md` §3/§4.7。机器门：`eng/tests/contracts/test_unified_object_contract.py`。

| schema_id | 内容 | scalar | shape/axis | unit | coordinate | invalid | ownership | serialization |
|---|---|---|---|---|---|---|---|---|
| DATA-OBJ-SIGNAL-001 | signal（UNIFIED_MODEL §2 对象；可否作权重：否） | f32|f64 | 对象文档 / 平面引用（map·scalar·control_points） | ADU/sr 或 BUNIT(声明) | 按对象（pixel / HEALPix NESTED / 源列表 / 元数据） | NaN / null | unique（canonical owner = eng/contracts/schemas/unified/signal.schema.json） | JSON schema + 产品载体（FITS/HiPS/JSON 由产品族决定） |
| DATA-OBJ-VARIANCE-001 | variance（UNIFIED_MODEL §2 对象；可否作权重：对该估计目标可以） | f32|f64 | 对象文档 / 平面引用（map·scalar·control_points） | signal单位^2（面亮度 ADU^2/sr^2） | 按对象（pixel / HEALPix NESTED / 源列表 / 元数据） | null（无覆盖）；负值/NaN=损坏 | unique（canonical owner = eng/contracts/schemas/unified/variance.schema.json） | JSON schema + 产品载体（FITS/HiPS/JSON 由产品族决定） |
| DATA-OBJ-IVAR-001 | ivar（UNIFIED_MODEL §2 对象；可否作权重：对该估计目标可以） | f32|f64 | 对象文档 / 平面引用（map·scalar·control_points） | 1/signal单位^2（面亮度 sr^2/ADU^2） | 按对象（pixel / HEALPix NESTED / 源列表 / 元数据） | 0=显式不可用；null=缺失 | unique（canonical owner = eng/contracts/schemas/unified/ivar.schema.json） | JSON schema + 产品载体（FITS/HiPS/JSON 由产品族决定） |
| DATA-OBJ-SOURCE-SNR-001 | source_snr（UNIFIED_MODEL §2 对象；可否作权重：不直接作帧权重） | f32|f64 | 对象文档 / 平面引用（map·scalar·control_points） | 1（F_hat/sigma_F 无量纲） | 按对象（pixel / HEALPix NESTED / 源列表 / 元数据） | null | unique（canonical owner = eng/contracts/schemas/unified/source_snr.schema.json） | JSON schema + 产品载体（FITS/HiPS/JSON 由产品族决定） |
| DATA-OBJ-DEPTH-M5-001 | depth_m5（UNIFIED_MODEL §2 对象；可否作权重：摘要，不作权重） | f32|f64 | 对象文档 / 平面引用（map·scalar·control_points） | mag | 按对象（pixel / HEALPix NESTED / 源列表 / 元数据） | null | unique（canonical owner = eng/contracts/schemas/unified/depth_m5.schema.json） | JSON schema + 产品载体（FITS/HiPS/JSON 由产品族决定） |
| DATA-OBJ-FRAME-SNR-001 | frame_snr（UNIFIED_MODEL §2 对象；可否作权重：唯一帧级参考；权重由 Phase2 逆方差叠加从 SNR 计算） | f32|f64 | 对象文档 / 平面引用（map·scalar·control_points） | 1（真实信号/噪声比） | 按对象（pixel / HEALPix NESTED / 源列表 / 元数据） | null | unique（canonical owner = eng/contracts/schemas/unified/frame_snr.schema.json） | JSON schema + 产品载体（FITS/HiPS/JSON 由产品族决定） |
| DATA-OBJ-POINT-INFORMATION-001 | point_information（UNIFIED_MODEL §2 对象；可否作权重：点源目标的严格权重） | f32|f64 | 对象文档 / 平面引用（map·scalar·control_points） | ADU^-2（=signal^-2） | 按对象（pixel / HEALPix NESTED / 源列表 / 元数据） | null | unique（canonical owner = eng/contracts/schemas/unified/point_information.schema.json） | JSON schema + 产品载体（FITS/HiPS/JSON 由产品族决定） |
| DATA-OBJ-PSFSW-ROBUST-WEIGHT-001 | **已退役**（14→13；变更 claim `CHG-2026-09-20-PSFSW-RETIRE`）：canonical schema 与 example **已删除**，对象不再存在；旧产品若声明该对象 ⇒ **显式拒绝 + 迁移提示**。本行只作**退役留痕**（原对象名 `psfsw_robust_weight`） | 已退役 | 已退役 | 已退役 | 已退役 | 已退役 | 已退役（原 canonical owner = `eng/contracts/schemas/unified/psfsw_robust_weight.schema.json（已删除）`，退役留痕） | 不适用（退役留痕；**不得**据此实现或验收） |
| DATA-OBJ-SPARSE-SNR-LAYER-001 | sparse_snr_layer（UNIFIED_MODEL §2 对象；可否作权重：帧内精细参考） | f32|f64 | 对象文档 / 平面引用（map·scalar·control_points） | 1 | 按对象（pixel / HEALPix NESTED / 源列表 / 元数据） | null | unique（canonical owner = eng/contracts/schemas/unified/sparse_snr_layer.schema.json） | JSON schema + 产品载体（FITS/HiPS/JSON 由产品族决定） |
| DATA-OBJ-SUPPORT-001 | support（UNIFIED_MODEL §2 对象；可否作权重：否） | f32|f64|int | 对象文档 / 平面引用（map·scalar·control_points） | 1（[0,1]） | 按对象（pixel / HEALPix NESTED / 源列表 / 元数据） | 0=无覆盖 | unique（canonical owner = eng/contracts/schemas/unified/support.schema.json） | JSON schema + 产品载体（FITS/HiPS/JSON 由产品族决定） |
| DATA-OBJ-COVERAGE-001 | coverage（UNIFIED_MODEL §2 对象；可否作权重：否） | f32|f64|int | 对象文档 / 平面引用（map·scalar·control_points） | 1（几何有效域） | 按对象（pixel / HEALPix NESTED / 源列表 / 元数据） | 0=无覆盖（空域） | unique（canonical owner = eng/contracts/schemas/unified/coverage.schema.json） | JSON schema + 产品载体（FITS/HiPS/JSON 由产品族决定） |
| DATA-OBJ-VALIDITY-001 | validity（UNIFIED_MODEL §2 对象；可否作权重：门，不是权重） | int | 对象文档 / 平面引用（map·scalar·control_points） | 1（状态量） | 按对象（pixel / HEALPix NESTED / 源列表 / 元数据） | missing=显式缺失态 | unique（canonical owner = eng/contracts/schemas/unified/validity.schema.json） | JSON schema + 产品载体（FITS/HiPS/JSON 由产品族决定） |
| DATA-OBJ-REJECTION-001 | rejection（UNIFIED_MODEL §2 对象；可否作权重：门/概率，不是 coverage） | f32|f64|int | 对象文档 / 平面引用（map·scalar·control_points） | 1（门/概率） | 按对象（pixel / HEALPix NESTED / 源列表 / 元数据） | 0=未拒绝（无覆盖=0） | unique（canonical owner = eng/contracts/schemas/unified/rejection.schema.json） | JSON schema + 产品载体（FITS/HiPS/JSON 由产品族决定） |
| DATA-OBJ-PROVENANCE-001 | provenance（UNIFIED_MODEL §2 对象；可否作权重：——） | int | 对象文档 / 平面引用（map·scalar·control_points） | 1（元数据，无量纲） | 按对象（pixel / HEALPix NESTED / 源列表 / 元数据） | unavailable.{flag,reason,scope} 显式登记 | unique（canonical owner = eng/contracts/schemas/unified/provenance.schema.json） | JSON schema + 产品载体（FITS/HiPS/JSON 由产品族决定） |

### 1.1 登记行权威锚点（CI-DATA-REG-001，2026-09-12）

上表末两行 `DATA-HIPS-001` / `DATA-TILE-001` 是**既存** ID 的登记补齐，
**不是新语义**：两 ID 早已在 DATA_SEMANTICS §29.5（"DATA-HIPS-001/
DATA-TILE-001（HiPS properties/tile 输入面）"）、生产 descriptor
（lib/infrastructure/scheduler/src/module_adapters.cpp:498-499 与 :459/:498）、
lib/infrastructure/pipeline/module_ports.registry.json:246/:277、registry 端口表
（docs/modules/registry/astrocs.phase3.resample.md、…resample2.md、
…properties.md）、eng/tests/unit/core_pipeline_test.cpp:66-67 在用，并在
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
| `weights` (integrate) | lib/algorithms/coverage/include/astro/phase2/integrate.h | 候选栈数值权重 = support×SNR² 或等权(1.0) | DATA-IMG-WEIGHT-001 数值权重 | 已消除(与 UPM 权重分离命名) |
| `weights` (rejection) | lib/algorithms/coverage/include/astro/phase2/rejection.h | 随样本携带到候选栈的数值权重 | 同上 | 已消除 |
| `upm.robust_control_weight` | upm.cpp | UPM 控制点权重 = quality×geom×control_ivar | DATA-UPM-CONTROL-UNC-001 | 已消除(禁止与 integration weight 混名) |
| `support` | integrate/upm/sampler | 覆盖支撑 [0,1] | DATA-IMG-SUPPORT-001 | 明确 |
| `scale_deg_per_px` | p3_session | 输出像元角尺度 | DATA-P3-FITS-001 s_out | 明确(单位 deg/px) |
| `sigma` | master_generator/rejection | MAD 转 σ 系数 1.482602218505602 / 拒绝阈值倍数 | SCI-NOISE/SCI-REJ | 明确(无量纲倍数) |
| `snr` | CW/sampler | 区域级 SNR 权重因子 snr_v² | SCI-CW-001 | 明确(与 ivar 语义分离) |
| `value` (integrate) | integrate.h | 候选样本值 | DATA-IMG-CAL-001 标度 | 明确 |
| `quality` | sampler | 帧/星点质量位掩码 | SCI-CW-001 | 明确 |
| `k_corr` | sampler.cpp:672 | Drizzle 相关校正 1.4 | SCI-UPM-WEIGHT-001 | 明确 |

结论：`weight` 在 integrate 与 UPM 两处语义已显式分离命名（`stack.*.v1` vs
`upm.robust_control_weight.v1`），无未消除歧义；`scale/sigma/snr/value/quality` 均有
明确 DATA/SCI 归属。G2 consumer API 可安全引用。

## 3. 机器校验

- `eng/tools/check_data_artifacts.py`（DATA-001 新增）：校验本表 schema_id 唯一、
  DATA-SEMANTICS.md 中声明的 DATA-* ID 全部在本表登记、无重复。
