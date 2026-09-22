# Phase3 目标态详细设计

> 上游：ASTROCS_DESIGN.md §6（export：投影导出）

文档 ID：`DESIGN-P3-001`  
状态：`TARGET_NORMATIVE`  
使命：把任意合同兼容 HiPS 科学产品按用户指定 WCS 导出为测量意义明确的平面 FITS；Phase3 是坐标/采样/格式变换，不重新发明上游科学权重。

## 1. 输入与模式

输入可为 Phase1 或 Phase2 产品。读取 properties、manifest、signal、variance/correlation、coverage/validity、PSF 或 point-source statistics。输出模式必须显式：

- `surface_brightness`：科学面亮度/扩展源；
- `point_source_flux`：Q/W/flux/detection 产品；
- `visualization`：允许显示型降级，但不得冒充测量产品。

- 输入 signal **只接受面亮度语义**（写端口单位 `SURFACE_BRIGHTNESS`，落盘值 = `flux_sum / covered_area`）；flux-per-pixel 等其它语义**显式拒绝**（`ASTROCS_DESIGN.md` §5.3/§5.6；正本见 `docs/science/PHASE3_HIPS_TO_FITS.md`）。
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
