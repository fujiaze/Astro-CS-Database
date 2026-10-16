# Export WCS 与裁剪几何

本节点从输出中心、尺寸、角尺度、方向与投影请求生成平面 WCS，供采样、writer 和 verify 共用。数学与输出被估量依 [HiPS 到 FITS 正本](../../science/projection/HIPS_TO_FITS.md)及 [Export 管线](../export/pipeline.md)。

## 实际入口和公开 API

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p3_op_wcs` 对应 `acsd.phase3.wcs`，读取 `p3_props.json`，调用 `p3_wcs_make` 并解析裁剪，保存 `p3_wcs.json`。生产 adapter 当前传 rotation_pa_deg=0；库有旋转参数不表示 CLI 已实际使用用户旋转。

公开头 [p3_wcs.h](../../../lib/algorithms/projection/p3_wcs.h)，实现 [p3_wcs.cpp](../../../lib/algorithms/projection/p3_wcs.cpp)。核心入口：

| 接口 | 作用 |
|---|---|
| `p3_wcs_validate_request` | 校验 projection/frame/coverage_output 的实际支持 |
| `p3_wcs_make` | centre、scale_deg_per_px、width/height、parity、PA → P3WcsDescriptor |
| `p3_wcs_pix2world/world2pix` | 零基像元中心 ↔ ICRS degree，返回具名状态 |
| `p3_wcs_fits_keywords` | 生成 CTYPE/CRPIX/CRVAL/CD/CUNIT 文本 |
| `p3_wcs_applicability`、`_check_applicability`、`_roundtrip_gate` | 当前实现范围与数值诊断，不能当投影理论的通用禁域 |
| `p3_crop_window_from_sky`、`p3_crop_apply_wcs` 等 | 唯一裁剪几何；不增加一次插值 |

投影注册在 [p3_projection_registry.h](../../../lib/algorithms/projection/p3_projection_registry.h)及其实现，当前生产仅 TAN。配置枚举中的 SIN/CAR/AIT 等不能列成已实现；不支持请求明确返回 UNSUPPORTED。

## 坐标、裁剪和单位

`P3WcsDescriptor` 的 CRVAL 为 ICRS degree，CD 为 degree/pixel，CRPIX 为 FITS 一基连续参考像元，width/height 为 px。变换入参/返回像元为零基中心；不重复加一，不再引入半像元。RA 归一与手性必须独立核对。

`P3CropWindow` 内部为零基半开区间 [x0,x1)×[y0,y1)。像素配置采用正式配置声明的一基闭区间，经唯一 adapter 转换；sky crop 由已定义 WCS 转整数窗口。两种形式互斥，越界/空窗明确报错。

子窗 CRVAL/CD 保持，CRPIX 减整数窗口原点，尺寸改为子窗尺寸。writer/verify 读同一 plan，裁剪不再重采样；因此外部合法 CRPIX 不必位于子窗中心。`p3_crop_window_from_sky` 采用有限密度边界采样，不能据此宣称对连续曲线极值已有严格包络保证。

## 所有权、错误和验证入口

descriptor/窗口由调用方持有，函数同步、无内部线程或文件 I/O，元数据 FP64；Runtime 管理节点与并行调用。状态为 OK、PARAM、UNSUPPORTED、HEMISPHERE，crop 有独立状态，不能直接冒作 CLI 退出整数。

当前 |Dec|、画幅/FOV 等检查是软件能力限制；TAN 奇点取决于距切点角距离。内部往返诊断不足以发现两向同错，必须对独立 WCS reader 作绝对坐标比较。

验证从本头 make/变换/crop 及 [WCS 单元入口](../../../eng/tests/unit/test_wcs_plausibility.py)进入：非零中心赤纬、RA wrap、旋转/手性、非对称裁剪、CRPIX 原点和未支持投影。已有函数/门限不等于完成外部 Oracle 或所有天区验证；本页未运行测试。
