# Phase3 投影/WCS 模块（astrocs.p3.projection）

> P3-PROJ-DOC 冻结（2026-09-11，SA-P3-P25）。模块总页；registry
> 手写合同页=docs/modules/registry/astrocs.phase3.wcs.md（行
> MOD-astrocs-phase3-wcs）；合同三件套=lib/phase3_proj/
> {README.md,module.yaml,memory.md}。

## 1 身份

- module_id: astrocs.p3.projection（MODULE_MIGRATION_MATRIX P3-PROJ
  行权威值）；registry 行 MOD-astrocs-phase3-wcs；
  dll_target=astrocs_p3_projection.dll（合同值，尚未存在，
  entrypoint=MISSING，由 P3-PROJ-IMPL 建立，禁止声明 IMPLEMENTED）；
  现状构建=astrocs_phase3_session 静态库成员（根 CMakeLists.txt
  :460-465）。
- owner: SA-P3-P25；language: c++17；abi_version: 1；
  phase_scope: phase3；resource_class: cpu_heavy；
  threading_model: host_executor_lease（迁移目标合同值；现状=内核
  纯函数无内部线程）。

## 2 合同链

| 层 | ID | 权威落位 | 状态 |
|---|---|---|---|
| SCI | SCI-P3-001 | docs/science/PHASE3_HIPS_TO_FITS.md（共享 FROZEN；映射声明 SCI-P3-WCS-001⇒SCI-P3-001 见 ALG §5） | FROZEN |
| ALG | ALG-P3-PROJ-IMPL-001 | docs/algorithms/PHASE3_PROJ_IMPL.md（兼承接 ALG-P3-002 G1/G2 本域子面） | CONTRACT_READY |
| DATA | DATA-P3-WCS | docs/contracts/DATA_SEMANTICS.md §28 | CONTRACT_READY |
| API | API-P3-PROJ-001 | docs/contracts/PUBLIC_API.md（Phase3 投影公共消费面节） | CONTRACT_READY |
| API(镜像) | API-P3-001 | PUBLIC_API.md（p3_session 五段编排面 FROZEN 镜像） | FROZEN 镜像 |
| ARCH | ARCH-001 | docs/architecture/cpu/ARCH_CONTRACTS.md | VERIFIED |
| TEST | TEST-P3-WCS-001 | 登记面=TEST-P3-WCS-DESIGN-001（ALG §12 + registry 页 §9 双重陈述 VERIFIED）；可执行面升级归 P3-PROJ-TEST | 见左 |
| EVID | EVID-MISSING | 归 P3-PROJ-INT/验收补 | MISSING |

## 3 职责

- TAN(gnomonic) 投影正/反映射（G2，pixel↔world，RA wrap 归一）；
  FITS WCS descriptor 构造（G1：CRPIX/CRVAL/CD、parity east_left/
  east_right、PA 推广 CD、手性 det=−s² 冻结）；FITS 关键词文本
  输出（CTYPE/CUNIT/CRPIX/CRVAL/CD）；极点/TAN 半球/参数守卫。
- 非职责: 重采样（phase3_resample2 域）、FITS 文件读写
  （phase3_fits 域）、会话编排（p3_session 域）、SIN/ZEA/CAR/AIT
  （扩展 TODO，SCI §9a-3 须独立测试+新 claim）。

## 4 生产源

- lib/phase3_session/p3_wcs.h（50 行，唯一权威签名头）+
  lib/phase3_session/p3_wcs.cpp（165 行）——六符号:
  P3WcsDescriptor/P3WcsStatus/p3_wcs_make/p3_wcs_pix2world/
  p3_wcs_world2pix/p3_wcs_fits_keywords。
- 会话消费点: p3_session.cpp :17/:160/:163/:232/:247-253；
  域际消费: p3_output.cpp:148-169（WCS 关键词写，DATA-P3-FITS 面）。
- 执行面: tests/unit/p3_wcs_test.cpp（90 行）+
  tests/backend/test_p1002_gaps.py（独立解析解回归）+
  tests/backend/p3_wcs_main.cpp（探针）。

## 5 端口与 DATA

| 端口 | DATA | 必/可 | 说明 |
|---|---|---|---|
| props | DATA-P3-PROPS | 必 | descriptor 词汇，HiPS properties 面 |
| wcs_plan | DATA-P3-WCS | 可 | §28 唯一权威（入参/映射/descriptor/关键词面） |

端口词汇为 descriptor 派生（module_adapters.cpp:344-361 占位
module_id=astrocs.phase3.wcs），由 P3-PROJ-INT 对齐，不作冻结依据。

## 6 实测偏差与整改（不修码）

- PA 未接线（p3_session.cpp:160 恒 0.0）→ P3-PROJ-IMPL/INT。
- kMaxSide=20000 可 ASTROCS_P3_MAX_SIDE 编译期覆盖（默认值语义）。
- projection 硬编码 "TAN"、UNSUPPORTED 枚举无产生点（扩展 TODO）。
- DLL/入口未建；WCSLIB 验收 oracle 归 P3-PROJ-TEST。
- 本域无 DISP 缺陷登记；详见 ALG-P3-PROJ-IMPL-001 §11/§13。

## 7 链接

- registry 手写页: docs/modules/registry/astrocs.phase3.wcs.md
- ALG: docs/algorithms/PHASE3_PROJ_IMPL.md
- DATA: docs/contracts/DATA_SEMANTICS.md §28
- API: docs/contracts/PUBLIC_API.md（API-P3-PROJ-001）
- SCI: docs/science/PHASE3_HIPS_TO_FITS.md（FROZEN，零改动）
- 同域: docs/modules/phase3_fits.md（写出域）、
  docs/modules/phase3_session.md（会话编排域）
