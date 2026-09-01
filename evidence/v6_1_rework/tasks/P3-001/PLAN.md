# P3-001: Phase3 科学与算法冻结

任务 ID: P3-001
Gate: G6
依赖: CHK-003
平台: Linux
变更类别: documentation + validation

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P3-001：

> 先修文档再修代码：明确 alpha 支持的 HiPS 版本/子集、NESTED、ICRS、TAN、单位、
> pixel center、CD parity、order、nearest/bilinear、coverage、NaN/missing、BITPIX、
> 最大资源。若排除距极点 5°，条件写为 `abs(dec)<=85°`；不允许 SCI/API/WCS/session
> 三套条件。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| HiPS 版本/子集/NESTED/ICRS/TAN | SCI-P3 文档 §1/§3a/§4(单通道 image HiPS, NESTED 唯一, ICRS, TAN) | c01 #2 |
| 单位/pixel center/CD parity | §3/§4/§9a(ADU 面亮度, 像素中心, CD-only, east_left parity) | c01 #2 |
| order/nearest/bilinear/coverage/NaN/BITPIX | §5/§8/§9a(order_sel, nearest+bilinear, coverage mask, NaN 传播, BITPIX=-32/-64) | c01 #2 |
| 最大资源 | FOV ≤20° 冻结(§9a.12) | c01 #2 |
| abs(dec)<=85° 单一条件 | SCI/API/session/WCS 四处统一(旧 |dec|≥5° 全部移除); session `fabs(dec)>85.0` 拒绝; WCS `kMaxAbsDec=85.0` | c01 #1 |
| 先文档后代码 | 文档(FROZEN)先行, 代码条件统一后测试 | c01 #4 |

## 实现文件

- `docs/science/PHASE3_HIPS_TO_FITS.md`：视场约束 → `abs(dec)<=85°`(单一条件)
- `docs/api/PHASE3_API_V1.md`：`|dec|≥5°` → `abs(dec)<=85°`
- `lib/phase3_session/p3_session.cpp`：`fabs(dec)<5.0` → `fabs(dec)>85.0` 拒绝
- `lib/phase3_session/p3_session.h`、`p3_wcs.h`、`p3_wcs.cpp`：统一 85° 表述(`kMaxAbsDec=85.0`)
- `tests/backend/test_p3001_science_freeze.py`（新）：4 组审计断言

## 测试结果

- `test_p3001_science_freeze.py`: 4/4 PASS
- `test_p3_resample.py`: 6/6 PASS; `test_p3_output.py`: 6/6 PASS(回归)

## 说明

- 统一后仅剩 `abs(dec)<=85°` 一个条件(SCI/API/session/WCS 一致), 无第二套。
- 文档状态 FROZEN(SCI-P3-001), 先文档后代码顺序满足。
