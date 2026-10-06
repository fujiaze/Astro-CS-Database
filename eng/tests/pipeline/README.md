# 阶段管线层测试集（`eng/tests/pipeline/`）

**这是工具，不是裁判。** 它产出的是**红项报告**，不是流水线判决。

依据：`standards/05_INDEPENDENT_TEST_SUITE.md` §1「测试代码是独立的一套代码集，
目标是主动暴露缺陷、检验科学口径，不作为门禁去约束代码」；§2 分层中 pipeline 层 =
阶段管线：normalize / mosaic / export 阶段内流程；§4「以非阻塞为常态」。
另见 `docs/engineering/testing/TEST.md` §9「测试集不产出流水线判决」。

## 运行

```bash
python3 -m eng.tests.pipeline.run_pipeline             # 报告；退出码恒为 0
python3 -m eng.tests.pipeline.run_pipeline --verbose   # 附带每条用例的实测读数
python3 -m eng.tests.pipeline.run_pipeline --list      # 只列元数据
python3 -m eng.tests.pipeline.run_pipeline --only norm # 只跑 id 含该词的用例
python3 -m eng.tests.pipeline.run_pipeline --exit-code # 开发期自查（不得接 CI）
```

## 分层里的位置

规范 §2 的六层里，本目录是**阶段管线层**：normalize / mosaic / export 各自阶段内
流程（调度→模块链→块生命周期→产物 manifest 落盘），不跨阶段、不跑全链。
跨阶段与全链由 synthetic / e2e 承接，**写域不重叠**。

## 目录

| 文件 | 内容 | 判据来源 |
|---|---|---|
| `tolerances.py` | **容差冻结表（唯一源）**。管线门限 + 性能六维记录档 | `TEST.md` §4；`05` §6 |
| `run_pipeline.py` | 报告器（非阻塞，退出码恒 0） | `TEST.md` §9；`05` §4 |
| `test_stage_normalize.py` | normalize 阶段：定标链、T01-D9/D10、release02 q1/phot_verify 吸收 | T01-D9/D10；release02 |
| `test_stage_mosaic.py` | mosaic 阶段：权重路由、排异边界、T01-D2/D3/D5/D11 | T01-D2/D3/D5/D11 |
| `test_stage_export.py` | export 阶段：投影往返、manifest 语义、T01-D6/D12 | T01-D6/D12 |
| `test_negative_36.py` | N01–N36 负例测试（每条一条负例） | T05 负向 36 项（构造待对拍） |
| `test_repro_ids.py` | T-CALIB/…/T-GUI 十一族复现锚点 | T10 复现编号（构造待对拍） |
| `test_perf_probe.py` | 性能六维：CPU/内存/扩展/IO/编排/缓存 | `05` §6 |

## 三条硬纪律（与 unit/synthetic 层同）

1. **期望值不得来自被测实现自身**：解析 Oracle / 高精度参考 / 定种子 Monte Carlo /
   第三方独立实现；容差写测试前冻结在本层 `tolerances.py`。
2. **负例必须真的能红**：注入缺陷后判据给出超界读数并记进 evidence；恒绿负例无效。
3. **条件化不变量不得写成无条件断言**：成立条件与反例同写。

## 吸收与删除记录

| 来源 | 吸收方式 | 状态 |
|---|---|---|
| T01-D2（weight_mode 路由） | `test_stage_mosaic.py::pipe.mosaic.weight_*` | 已落为测试 |
| T01-D3（UPM 份额式） | `test_stage_mosaic.py::pipe.mosaic.upm_*` | 已落为测试 |
| T01-D4（k_corr≥1） | `test_stage_normalize.py::pipe.norm.kcorr_*`（unit 层已有边界负例，本层落阶段面） | 已落为测试 |
| T01-D5（排异边界 n=3/4/5/6） | `test_stage_mosaic.py::pipe.mosaic.reject_*` | 已落为测试 |
| T01-D6（null model NaN） | `test_stage_export.py::pipe.export.null_*` | 已落为测试 |
| T01-D7（worker 无关性 1e-12） | `test_stage_mosaic.py::pipe.mosaic.worker_*`（integration 层已有收紧测试，本层落阶段面） | 已落为测试 |
| T01-D8（min_samples/ivar 单位） | `test_stage_normalize.py::pipe.norm.noise_*` | 已落为测试 |
| T01-D9（gain 方向/常数） | `test_stage_normalize.py::pipe.norm.gain_*` | 已落为测试 |
| T01-D10（min_samples 根因） | `test_stage_normalize.py::pipe.norm.ls_*`（science 重写吸收，本层落可执行面） | 已落为测试 |
| T01-D11（UPM ivar 回退/子产品缺失） | `test_stage_mosaic.py::pipe.mosaic.upm_fallback_*` | 已落为测试 |
| T01-D12（接缝/负值/判面） | `test_stage_export.py::pipe.export.seam_*` + e2e 视觉 | 已落为测试 |
| T01-D1（归一化权重 g²） | synthetic 负例（乘性标度翻转序）+ UNRESOLVED 候选 | 本层登记，不落数值门 |
| T01-D13（门禁正当性） | 旧表述随文件删除，不吸收（清单已裁决） | 不吸收 |
| release02 q1/q2/q3/phot_verify/c_delta/unc | 重写吸收（有 Oracle + 负例，不原样搬运） | 见各阶段文件头 |
| quality 工具（compare/frame_qc/resource/plane 切块拉伸） | 诊断工具吸收为 Oracle/探针引用，不搬运为判据 | 见 `test_perf_probe.py` / e2e |
| T05 N01–N36 | `test_negative_36.py` 每条一条负例 | 构造待对拍（T05 输出未落仓） |
| T10 十一族 | `test_repro_ids.py` 每族一锚点 | 构造待对拍（T10 输出未落仓） |

⚠ N01–N36 与 T10 十一族的编号是**构造的待对拍锚点**：T05/T10 输出尚未落仓，
本层按科学域先行构造 36 条负例与 11 个锚点，待 T05/T10 输出落仓后逐条对拍；
对拍前不得引用为证据。
