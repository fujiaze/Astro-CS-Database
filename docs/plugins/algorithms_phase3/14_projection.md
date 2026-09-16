# 插件文档：projection（投影 registry）

## 1. 职责与边界

- **职责**：管理 export 支持的 WCS 投影 registry（**内置多种投影算法**：TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA），定义适用域、奇点、坐标语义与正反变换合同。
- **不是**：不做重采样（resample）；不做 FITS 写出（fits_output）；新增投影必须注册并附独立 Oracle。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §5.3（投影算法：内置多种）
- `docs/design/PHASE3_DETAILED_DESIGN.md` §2
- FITS WCS Paper I/II（外部标准）

## 3. 输入/输出数据合同

- **输入**：用户 WCS 计划（中心、尺度、shape、旋转、投影或足够约束）。
- **输出**：注册的投影定义（CTYPE、正反变换、适用域、奇点处理、CRPIX/CRVAL/CD/PC/CDELT）。
- 参考：`contracts/schemas/projection_registry.schema.json`。

## 4. 算法与公式要点

- **内置多种投影**，首批冻结 **TAN / SIN / CAR / AIT / STG / MOL / CEA / ZEA**，每种声明：适用域、奇点、经度 wrap、轴手性、CRPIX/CRVAL/CD/PC/CDELT、CTYPE；
- FITS 1-based 关键字、内部 0-based 像素中心，转换唯一；
- 正反变换必须互逆（误差 < 合同阈值）；
- 新增投影经 registry 注册并附独立往返 Oracle。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `projection` | `tan` | —— | tan/sin/car/ait/stg/mol/cea/zea |
| `crpix` | 中心 | px | 参考像素 |
| `crval` | —— | deg | 参考天球坐标 |
| `cd_matrix` / `cdelt` | —— | deg/px | 尺度 |
| `rotation` | 0 | deg | 旋转（若用 CD） |

## 6. 接口/ABI

- entrypoint：注册表查询 + 正反变换调用；
- resample 调用本模块获取变换。

## 7. 错误与边界

- 未注册投影 → 拒绝；
- 超适用域（极点/奇点）→ 明确处理（wrap/拒绝），不得静默错位；
- 轴手性/CRPIX 单位错误 → fail-closed。

## 8. 测试与 Oracle

- Astropy/WCSLIB 独立正反投影往返（中心、边、wrap、极点、奇点）；
- **八投影全覆盖测试**；
- 新增投影必须带独立 Oracle 才可注册。
