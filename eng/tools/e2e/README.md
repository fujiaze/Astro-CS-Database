# e2e

三命令真实数据全链驱动与 L4 视觉检查：端到端配置生成、链条复验与成品渲染。

## 职责边界

- 放：三命令配置生成、normalize 到 mosaic 到 export 串行执行与 manifest 链独立复算、成品帧渲染自检。
- 不放：三阶段实现（在 `lib/`）。

## 内容

- `run_e2e_chain.py` —— 端到端链条驱动：串行跑三命令，独立复算阶段间 manifest 哈希链，核对产品结构、覆盖与目录纪律，分段计时；`--verify-only` 与 `--self-test` 为自检入口。
- `make_e2e_configs.py` —— 从真实帧构造三命令配置，字段取自命令面合同。
- `make_vis_configs.py` —— 两组成品帧渲染配置生成。
- `render_vis.py` —— 成品帧渲染与逐块自检：覆盖域自洽、非零占比、动态范围与渲染分块边界比值，附 `--self-test`。
- `__pycache__/` —— Python 字节码缓存，不入库。

## 上游

- 真实数据档流水线见 `docs/engineering/02_PIPELINE.md`，证据落位见 `docs/engineering/04_ARTIFACTS.md`。
