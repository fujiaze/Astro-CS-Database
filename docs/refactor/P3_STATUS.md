# P3-001: Phase3 production registry/preset 状态

状态: **PASS** — HEAD=`f5e1177`
规则: 从 production registry/preset 移除 prototype; 文档状态统一 NOT_IMPLEMENTED/PROTOTYPE; 保留源码供定向参考, 不做破坏性删除。

## Production registry 现状 (CLI 命令表)
- `astrocs phase3` 子命令注册于 cli/main.cpp (CLI-005), 指向 lib/phase3_session (正式实现)。
- **无 prototype 进入生产 registry/preset**: 代码层 `grep prototype` 扫描 lib/cli = 0 命中。
- phase3 正式模块: p3_wcs / p3_resample / p3_output / p3_session (facade) / hips_properties。

## 文档状态统一
| 项 | 旧状态 | 新统一状态 |
|---|---|---|
| phase3_session 旧单线程实现 | PROTOTYPE_NOT_PRODUCTION (SCIENCE_OVERVIEW §现状) | **PROTOTYPE** (参考保留, 不破坏删除) |
| phase3 正式模块 (p3_wcs/resample/output) | — | **IMPLEMENTED** (生产) |
| ACR 路径 | — | **NOT_IMPLEMENTED** (生产禁, LEG-004) |
| cuda_bridge_stub | — | **PROTOTYPE** (仅编译占位) |

## 保留原则
- 旧 prototype 源码完整保留 (lib/phase3_session 历史实现), 不做破坏性删除, 仅供定向参考。
- production registry/preset 只注册正式模块, 不引用 prototype。

## 验证
- `grep -rn prototype lib/ cli/` (除 test/third_party) = 0 命中
- cli/main.cpp phase3 命令指向正式模块 (p3_*)
- INDEX.yaml 无 phase3 prototype 条目为 ACTIVE
