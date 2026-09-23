# Phase3 目标态详细设计

> 上游：ASTROCS_DESIGN.md §6（export：投影导出）

使命：把任意合同兼容 HiPS 科学产品按用户指定 WCS 导出为测量意义明确的平面 FITS；Phase3 是坐标/采样/格式变换，不重新发明上游科学权重。

## 1. 输入与模式

输入可为 Phase1 或 Phase2 产品。读取 properties、manifest、signal、variance/correlation、coverage/validity、PSF 或 point-source statistics。输出模式必须显式：

- `surface_brightness`：科学面亮度/扩展源；
- `point_source_flux`：Q/W/flux/detection 产品；
- `visualization`：允许显示型降级，但不得冒充测量产品。

- 输入 signal **只接受面亮度语义**（写端口单位 `SURFACE_BRIGHTNESS`，落盘值 = `flux_sum / covered_area`）；flux-per-pixel 等其它语义**显式拒绝**（`ASTROCS_DESIGN.md` §5.3/§5.6；正本见 `docs/science/PHASE3_HIPS_TO_FITS.md`）。
- 输入的 signal 是**线性**面亮度（Phase1 面亮度产品为 `ADU/sr`）：本阶段只做坐标/采样/格式变换，采样核是输入的**凸组合**，因此**不改量纲类别、不做星等换算**；输出 `BUNIT` 透传输入语义，variance/ivar 按二次律同幂传播（`docs/contracts/DATA_SEMANTICS.md` §31.1a/§31.2）。
- 输入产品的落盘形态由落盘名判定，裸 `<name>.hips/` 与归档 `<name>.hips.zst` 语义相同，按天区取瓦片走输入产品的覆盖索引（`docs/design/PRODUCT_STORAGE_FORM.md`）。
- **Phase3 产物是交付物**：输出为**裸 FITS 文件，不压缩、不套壳**（用户与外部工具直接打开）；Phase3 不产出 HiPS，因此不使用 `.hips` / `.hips.zst` 命名。输入合同**不设** `storage_form` 键（形态由落盘名判定、对调用方透明），出现即 REJECT。

缺少所选模式所需的不确定度/PSF 信息时拒绝或明确输出 unavailable。

## 2. WCS 计划

用户提供中心、尺度、shape、旋转、投影或足够约束。**冻结清单 = 8 种**（TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA）；**已实现并可作为产品声明的以实际注册表为准——当前仅 TAN**，**未实现的必须显式报「不支持」**，**禁止**声称支持（`ASTROCS_DESIGN.md` §5.3）。每种定义适用域、奇点、经度 wrap、轴手性、CRPIX/CRVAL/CD/PC/CDELT 和 CTYPE。采用 FITS 1-based 关键字、内部 0-based 像素中心，转换唯一。

计划阶段：

1. 验证输出足迹和投影有效域；
2. 选择满足 Nyquist/信息损失约束的输入 order；
3. 估算 tile、内存、I/O、缺失域；
4. 冻结 run_manifest 后执行。

## 3. 反向映射与采样

对每个输出像素中心，WCS inverse 得到 sky，再 HEALPix 定位输入。科学模式要求球面几何一致，不允许平面距离替代。采样核是产品语义：

- nearest 仅离散 mask/诊断或显式用户选择；
- bilinear/高阶核用于连续场，但必须说明通量/面亮度语义；
- point-source Q、W、PSF 参数不得把普通 signal 插值规则机械套用；
- coverage、variance、correlation 分别传播；非有限/缺 tile 不以零填充。

当前“四象限最近中心双线性”只能在独立 Oracle 和误差/边界定义后作为一个注册核；不能因现码存在就成为永恒目标。

## 4. 不确定度与 PSF

线性采样 `y = R x` 时：

```text
C_y = R C_x Rᵀ
```

若仅输出对角 variance，必须同时给相关核/近似误差；coverage 不得代替 variance。PSF 随同采样算子传播，输出 effective PSF 或明确无法支持点源测量。Q/W 产品的重采样必须保持信息解释，并用注入源验证。

## 5. FITS 产品

- PRIMARY：所选科学 signal/flux/statistic；
- 扩展 HDU：VARIANCE/IVAR（语义择一且一致）、COVERAGE、VALIDITY、SUPPORT、REJECTION（若存在）、POINT_INFORMATION/W（若模式需要）；
- PSF 表/图和 correlation 描述；
- 标准 WCS、BUNIT（面亮度语义，写端口单位 `SURFACE_BRIGHTNESS`）、DATASUM/CHECKSUM；
- provenance：源 product/hash、软件完整 SHA、配置、投影、核、order、近似和生成时间。

所有 HDU shape/WCS 对齐。写临时文件、flush/close/fsync、标准 checksum、原子 rename、重开独立验证；失败/取消无可见半成品。

## 6. 流式与资源

输出按行带/块执行，内存 `O(width × band_height + tile_cache)`，不允许整幅超大图常驻。cache 只缓存，不改变 order/核/科学值。线程预算来自 Runtime；并行输出与单线程科学结果一致。

## 7. 验收

- Astropy/WCSLIB 独立正反投影，TAN/SIN/CAR/AIT 覆盖中心、边、wrap、极点和奇点；
- HEALPix/HiPS 外部实现交叉；跨 tile 连续场无缝；
- 常量面亮度、点源通量、variance/correlation 传播与注入源恢复；
- 不同 block/cache/worker 输出科学值一致；
- >2 GiB、长 UTF-8 路径、Windows CFITSIO、取消和缺 tile；
- 只要输出缺失其声明的科学层，就不得标 VERIFIED。

## 8. 导出裁剪范围（crop）

> 权威：负责人裁决 2026-09-23 逐字：「默认导出的话是要求边框不得裁剪任何有效像素，然后可以
> 导出一些黑边。到平面后我自己手动剪裁。然后支持手动输入裁剪范围。这样我以后 gui 的 HiPS
> 浏览器里面我可以直接导出框选。需要保留接口。」
> 上游：`ASTROCS_DESIGN.md` §6（export 按用户指定 WCS 导出平面 FITS）。

### 8.1 语义

- **默认不裁剪**：配置里没有 `crop` 键 ⇒ 导出**整幅**请求画幅，行为与既有导出逐位相同。
  请求画幅可以大于数据足迹，此时边框上自然出现**非有限黑边**——这是**设计允许**的
  （导出不得裁剪任何有效像素；用户到平面后自行裁剪）。视觉验收判据因此只禁**内部空洞**
  （被有限值包围的非有限像素），不禁边界黑边（`ACCEPTANCE_SPEC.md` §6.2「无"黑洞"」）。
- **裁剪 = 在已定义好的输出画幅上取矩形子窗**。画幅仍由 `center` / `width_px` /
  `height_px` / `scale_deg_per_px` / `longitude_parity` / `projection` 定义；`crop`
  **不改画幅定义、不改投影、不改重采样**，只决定最终写出的 FITS 覆盖画幅的哪一块。
- **精确性（关键判据）**：裁剪帧的 WCS 必须是原画幅 WCS 在窗口上的**精确限制** ——
  `CRVAL`/`CD` 逐位不变、`CRPIX` 减去**整数**窗口原点（无重投影、无二次插值）。
  由此「裁剪框内的像素与不裁剪时**逐位相同**」由构造保证：**裁剪只能裁，不能改数值**。

### 8.2 两种输入形式（互斥）

| `crop_form` | 形式 | 坐标约定 |
|---|---|---|
| `pixels` | 平面像素矩形 | FITS 1-based **闭区间** `[x0,x1]x[y0,y1]`（与 `crpix_px` 同约定），相对**未裁剪输出画幅**；窗口 = `(x1-x0+1)x(y1-y0+1)`。用户到平面后手动裁剪；GUI 框选也是平面矩形 |
| `sky` | 天球轴对齐矩形 | ICRS deg `[ra_min_deg,ra_max_deg]x[dec_min_deg,dec_max_deg]`；`ra_min_deg > ra_max_deg` = 跨 RA=0 绕回。GUI 的框选来自 HiPS 浏览器，框的是**天区** |

- 两形式**互斥**：`crop_form` 选中其一，另一形式同时出现即**具名拒绝**（不比较、不取一）。
- `sky` 的转换 = 边界**加密采样**（每边 257 点，确定性）→ 逐点 `world2pix` → **外扩**整数
  包围盒（`floor(min)` / `floor(max)+1`）⇒ 保证不切掉任何**中心落在矩形内**的像素。
- 判别键名取 `crop_form` 而非 `mode`：`cpu_profile` 的 `legacy_v1`/`kernel_v1` 已占用
  `mode`，同名异义被 `docs/design/UNIFIED_MODEL.md` §3 禁止（与 export 用 `output_mode`
  避开 `mode` 同一处置）。

### 8.3 fail-closed 判据（全部具名报错，禁静默夹取）

| 情形 | 判据 |
|---|---|
| 越界 | `pixels` 超出 `[1,width_px]x[1,height_px]`；或 `sky` 转换后的窗口超出画幅 ⇒ 拒绝 |
| 宽高非正 | `x1<x0` / `y1<y0` / `dec_min_deg>=dec_max_deg` / `ra_min_deg==ra_max_deg` ⇒ 拒绝 |
| 两种形式同时给 | `crop_form` 与其未选中的形式同时出现 ⇒ 拒绝（不比较、不取一） |
| 裁剪后为空 | 转换后窗口零面积或空集 ⇒ 拒绝 |
| 适用域 | `sky` 边界点落在 TAN 半球之外 / abs(dec)>85° ⇒ 拒绝（继承 §2 的 TAN 冻结域） |

夹取（clamp）被禁止：把「用户以为裁到了」变成静默错图，比拒绝更危险。

### 8.4 落点与不变式

- **几何唯一实现** = `lib/algorithms/projection/p3_wcs.h`（header-only）：CLI 配置面与
  scheduler 节点面共用，禁第二份（与 `p3_wcs_validate_request` 同一「唯一语义源」处置）。
- **生产消费点** = scheduler 的 p3 节点链：`wcs` 节点解析窗口并随 `p3_wcs.json` 落盘；
  `writer` 只按窗口写出（流式读起点按窗口原点平移）；`verify` 独立重开对**裁剪后画幅**
  逐像素对拍。`resample` **不变**（仍按未裁剪画幅产出平面）⇒ 窗口内像素与不裁剪逐位相同。
- **接口稳定性**：键形固定、可机器生成，GUI 的 HiPS 浏览器框选导出将来直接填该键
  （登记见 `docs/api/CLI_PROTOCOL_V1.md` §7.1；字段合同见
  `eng/contracts/schemas/phase_config_export.schema.json#/$defs/export_crop`）。

## 9. 导出产品的视觉验收判据（V1a / V1b）

工具 = `eng/tools/e2e/render_vis.py`（整幅 PNG + 分块 PNG + 逐块自检 + 接缝度量）；
上游 = `ACCEPTANCE_SPEC.md` §6.2「无"黑洞"：无异常零值/死区/未填充孔洞」+ §6 L4
（真实数据端到端视觉验收）+ 负责人裁决 2026-09-23（默认导出不得裁剪任何有效像素，
**允许**导出黑边，由用户到平面后自行裁剪）。

### 9.1 V1a：未覆盖却有值（幻影数据）

`COVERAGE == 0` 而 SIGNAL 有限 ⇒ 判红（`phantom_data_px > 0`）。覆盖平面语义 =
**足迹内有 tile 像素**（裁决确认，见 §8）；未覆盖处出现有限值只能是幻影数据。

### 9.2 V1b：内部空洞（阈值 0）

判据 = **不与画幅边界连通的非有限像素连通域数 == 0**（4-连通；`internal_hole_px`、
`internal_hole_components`、`internal_hole_largest_px`）。

**为什么不是「覆盖却非有限」**：导出画幅由用户给定，**可以大于数据足迹** —— 此时边框上
必然出现非有限黑边，这是设计允许的几何（§8）。因此「覆盖 > 0 ⇒ 必须有限」这个前提是
**错的**：它把「画幅大于足迹」这一合法几何误判成缺陷。M42 全画幅导出实测 464,263 px
（2.77%）被判红，而它们**全部**与画幅边界连通（`boundary_nonfinite_px = 520,014`、
`internal_hole_px = 0`）。真正该判的是被**有限值包围**的非有限像素 = 内部空洞。

阈值出处：`ACCEPTANCE_SPEC.md` §6.2 的「未填充孔洞」逐字对应内部空洞 ⇒ 阈值 **0**
（一个像素也算缺陷），与「边界黑边」的合法裕度互不冲突。

**无覆盖平面时的同一口径**：产品没有 COVERAGE 平面时，V1a 不适用，V1b 用**同一条**
内部空洞规则（旧实现按 `finite_fraction != 1.0` 判红 = 与边界黑边冲突，已撤除）。

### 9.3 可核对量（`vis_report.json#coverage_crosscheck`）

| 字段 | 含义 |
|---|---|
| `boundary_nonfinite_px` | 与画幅边界连通的非有限像素数（合法黑边） |
| `internal_hole_px` / `_components` / `_largest_px` | 内部空洞像素数 / 连通域数 / 最大连通域 |
| `covered_fraction` | COVERAGE > 0 的占比（= 足迹覆盖率，**不是**有效率） |
| `phantom_data_px` | 未覆盖却有值的像素数（V1a 判据量） |
| `covered_but_nonfinite_px` | 覆盖却非有限的像素数（**仅诊断**，不判红） |

`covered_fraction` 与 `finite_fraction` 的差就是「覆盖 ≠ 有效数据」的量：M42 实测
`0.99668` vs `0.96900`，差值 464,263 px。判据不因该差值判红（见 §9.2），但两者必须
同时落盘，便于独立复核。

