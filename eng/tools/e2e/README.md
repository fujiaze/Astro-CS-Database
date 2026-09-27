# e2e

三命令真实数据全链驱动与 L4 视觉检查：端到端配置生成、链条复验、成品渲染与帧足迹接缝判据。

## 职责边界

- 放：三命令配置生成、normalize 到 mosaic 到 export 串行执行与 manifest 链独立复算、成品帧渲染自检、帧足迹边界电平阶跃判据。
- 不放：三阶段实现（在 `lib/`）、验收标准正文（在 `ACCEPTANCE_SPEC.md`）、检查项注册表（在 `eng/ci/checks.json`）。

## 内容

- `run_e2e_chain.py` —— 端到端链条驱动：串行跑三命令，独立复算阶段间 manifest 哈希链，核对产品结构、覆盖与目录纪律，分段计时；`--verify-only` 与 `--self-test` 为注册入口。
- `make_e2e_configs.py` —— 从真实帧构造三命令配置，字段取自命令面合同。
- `make_vis_configs.py` —— 两组成品帧渲染配置生成。
- `render_vis.py` —— 成品帧渲染与逐块自检：覆盖域自洽、非零占比、动态范围与渲染分块边界比值，附 `--self-test`。
- `seam_footprint.py` —— 沿真实帧足迹边界的电平阶跃接缝门，阈值取自实验文档的相对接缝度量口径，内建正负例。
- `__pycache__/` —— Python 字节码缓存，不入库。

## 上游

- 注册于 `eng/ci/checks.json`：`CHK-E2E-CHAIN`（`E2E-CHAIN-VERIFY-STEP`）、`CHK-E2E-CHAIN-SELFTEST`（`E2E-CHAIN-SELFTEST-STEP`）、`CHK-L4-SEAM-FOOTPRINT`（`SEAM-FOOTPRINT-SELFTEST`、`SEAM-FOOTPRINT-PRODUCT`、`RENDER-VIS-V6-SELFTEST`）。
- 检查项条目见 `docs/ci/01_CHECKS.md`，真实数据档流水线见 `docs/ci/02_PIPELINE.md`，证据落位见 `docs/ci/04_ARTIFACTS.md`。
