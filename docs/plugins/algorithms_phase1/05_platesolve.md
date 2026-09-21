# 插件文档：platesolve（天体测量/WCS）

## 1. 职责与边界

- **职责**：从检测源与参考星表（Gaia/离线）解算天体测量解，生成 ICRS WCS 并验证。
- **不是**：不做背景/噪声估计；不做测光；WCS 是坐标合同，不是权重。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §4.6（硬约束：WCS 为 ICRS）与 §4.2（WCS 两轮解算）
- `docs/design/PHASE1_DETAILED_DESIGN.md` §7（天体测量与测光）
- `docs/algorithms/PLATESOLVE.md`（解算算法推导）
- `docs/plugins/infrastructure/22_gaia_xpsd_client.md`（星表查询依赖）

## 3. 输入/输出数据合同

- **输入**：检测目录（像素坐标）、参考星表匹配集、初始猜测（可空）、配置。
- **输出**：WCS（ICRS，像素中心/轴向/单位/SIP/PV 域明确）、匹配表、残差统计、验证记录。
- 正反变换一致，独立星表残差验证。第一轮盲检测粗解只为星表逆映射提供近似指向；第二轮用星表引导检测后的高纯度星表精解，精解结果才是权威 WCS（最高设计 §4.2）。
- 参考：`contracts/schemas/wcs_output.schema.json`。

## 4. 算法与公式要点

- 匹配：像素→天球→匹配（几何+亮度辅助），拒绝离群；
- 拟合：TAN 基（或按需 SIP/PV），最小二乘 + 稳健迭代；
- 输出：CRPIX/CRVAL/CD/PC/CDELT/CTYPE，FITS 1-based 关键字、内部 0-based 像素中心；
- 系统误差与随机误差分开报告（与测光一致）。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `catalog` | `gaia` | —— | 星表源（gaia/离线） |
| `catalog_version` | —— | —— | 星表版本（必填） |
| `tweak_order` | `tan` | —— | WCS 模型（tan/sip/pv） |
| `max_matches` | 200 | —— | 匹配上限 |
| `residual_gate` | —— | mas/px | 残差门（拒绝） |

## 6. 接口/ABI

- entrypoint：目录+星表 → WCS+匹配+残差；
- 与 gaia_xpsd_client 的查询/缓存/坐标语义合同联动。

## 7. 错误与边界

- 匹配不足/无法收敛 → fail-closed（不得输出伪 WCS）；
- 残差超门 → 拒绝或标记，不得"尽力拟合"；
- 经度 wrap、极点、轴手性按投影规则处理。

## 8. 测试与 Oracle

- Astropy/WCSLIB 独立正反投影往返一致；
- 注入已知 WCS 图像 → 解算残差符合理论；
- 离群匹配拒绝测试；
- 跨 tile 一致性（多帧同场 WCS 一致性）。
