# 模块 astrocs.phase1.star-psf

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 合同：SCI-P1-PSF-001（docs/science/algorithms/STAR_PSF_ALGORITHMS.md §11.5）/
> ALG-STARPSF-001（§11.1）/ DATA-P1-PSF（DATA_SEMANTICS §15）/ API-PSF-001
> （PUBLIC_API PSF 节）。模块级事实以 lib/algorithms/psf/README.md（CONTRACT_READY）+
> lib/algorithms/psf/module.yaml（astrocs.p1.psf，迁移目标 astrocs_p1_psf.dll）为准；
> 现状构建 Makefile:3-5 → dynamic_psf.dll，未编入根 CMake 主构建。测试设计
> TEST-PSF-DESIGN-001（STAR_PSF_ALGORITHMS §11.4）已冻结；可执行测试待建
> （TEST-P1-PSF-001）。缺陷登记见 STAR_PSF_ALGORITHMS.md §11.3。

## 职责与明确非职责

Registry production 模块（唯一源 = module_adapters.cpp 的 p1_star_psf_descriptor）。
职责：动态 PSF 建模与拟合质量代理（Moffat4 参数拟合、θ 消歧、q_psf 质量指标）——
生产源 lib/algorithms/psf/（dynamic_psf.dll），契约见 STAR_PSF_ALGORITHMS.md。
不做：星点检测（P1-STAR）、测光定标（P1-PHOT）、盘面 I/O。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
| `cleaned` | `DATA-P1-COSMETIC` | 必 | `UnitId::ADU` | `CoordinateFrame::PIXEL` |
| `wcs` | `DATA-P1-WCS` | 必 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `sources` | `DATA-P1-SOURCES` | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::ICRS` |
| `psf` | `DATA-P1-PSF` | 可 | `UnitId::DIMENSIONLESS` | `CoordinateFrame::PIXEL` |

invalid = NaN/coverage=0(按 DATA 合同)。

## 公共 header、核心 symbol 与生命周期

由 `API-P1-003` 公共 API 定义(phase session extern "C"); 生命周期 create→validate→
run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`astrocs.phase1.star-psf`; execution_class=`cpu_heavy`;
parallel_ok=True; 配置=phase config JSON(按 PHASE API 文档)。

## Execution class、并行轴、ThreadBudget lease、确定性

`cpu_heavy`; parallel=是(资源门拒绝 heavy+serial 组合); worker 数=ThreadBudget.max_workers(唯一取值源);
确定性=NOT_VERIFIED（未取得验收证据；不写成 PASS/FAIL）。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

错误码与退出码唯一源=lib/infrastructure/cli/exit_codes.h（本页不复制数值表）；取消=协作取消（契约：宿主 cancel 通道 → 停止调度新单元 → 等运行中单元完成 → exit 9，最高设计 §6.3；接线以实测为准）；模块内无 checkpoint（无断点续算）。

## 独立 synthetic 验证命令与容差

测试标识=`TEST-P1-PSF-001`（registry descriptor 单源）；执行证据=NOT_VERIFIED（未取得验收证据）；容差=NOT_VERIFIED（同源）。
测试设计=TEST-PSF-DESIGN-001（STAR_PSF_ALGORITHMS §11.4：四状态码负例/
解析 Moffat4 oracle/θ 消歧确定性/NaN 占位一致性，逐码逐锚），fixture 生成器
注记容差来源。

## 已知限制

见 docs/KNOWN_LIMITATIONS.md 与 `ALG-STARPSF-001` 合同边界（缺陷登记 =
STAR_PSF_ALGORITHMS.md §11.3：covariance 缺口、maxIter/tolerance 死参数）。
