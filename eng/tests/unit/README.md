# 单元层测试集（`eng/tests/unit/`）

**这是工具，不是裁判。** 它产出的是**红项报告**，不是流水线判决。

依据（逐字）：`run/GOVERN-08/工作包-RECTIFY-09原件/standards/05_INDEPENDENT_TEST_SUITE.md` §1
「测试代码是独立的一套代码集，目标是主动暴露缺陷、检验科学口径，**不作为门禁去约束代码**」；
§1「**测试是工具不是裁判**：帮助发现问题，不堆叠豁免、不用空断言充数」；§4「测试集可以接入
CI，但**以非阻塞为常态**」。另见 `docs/engineering/testing/TEST.md` §9 逐字
「执行者是人：结论由人读对抗审核给出，每条结论都附可复算的证据。**测试集不产出流水线判决**。」

## 运行

```bash
python3 -m eng.tests.unit.run_unit                  # 报告；退出码**恒为 0**
python3 -m eng.tests.unit.run_unit --verbose        # 附带每条用例的实测读数
python3 -m eng.tests.unit.run_unit --list           # 只列元数据（意图/输入/预期/来源/注入）
python3 -m eng.tests.unit.run_unit --only drizzle   # 只跑 id 含该词的用例
python3 -m eng.tests.unit.run_unit --exit-code      # 开发期自查：退出码 = 红项数（**不得接 CI**）
```

退出码默认恒 `0` 是**刻意设计**，由 `test_criterion_meta.py::meta.verdict_is_non_blocking_even_with_failures`
机器守卫：想把它改成有红项即 `red` 的那条测试会立刻变红。

## 分层里的位置

规范 §2 的六层里，本目录是**单元层**：函数与核，含 Oracle 对拍与归零负例。
`module/` `integration/` `pipeline/` `synthetic/` `e2e/` 由其它车道负责，**写域不重叠**。

## 目录

| 文件 | 内容 | 判据来源 |
|---|---|---|
| `tolerances.py` | **容差冻结表（唯一源）**。每条带值、适用量级域 `scale`、出处 | `TEST.md` §4/§4.1/§4.3 |
| `harness.py` | 极小骨架：注册表 + 断言 + 证据累积 + 非阻塞裁决 | `05_INDEPENDENT_TEST_SUITE.md` §1/§4 |
| `run_unit.py` | 报告器（非阻塞） | `TEST.md` §9 |
| `test_criterion_meta.py` | **本测试集自身的判据**：S17 三条元口径的可执行落法、E4/E5/E6/E13、`TEST.md` §4.1/§4.3/§4.4/§5 | 审核包-R2 §2.1 S17、§2.4 |
| `test_resource_judgement.py` | 资源判据，**直接对拍在库产品实现本体** | 审核包-R2 §2.2 P5/P7/P10/P11/P12、§2.3 C3、§2.4 E1/E3/E6/E9 |
| `test_science_gate.py` | 测光双边界门（S6）+ 三条「就地即正本」判据（S14/S15/S16）的可执行落法 | `实验/photometric-magnitude/README.md`、审核包-R2 §2.1 |
| `test_rejection_routing.py` | 排异路由（S1/S2 成对、S3 否定式、S4 耦合不变量） | `docs/science/REJECTION.md` §5 |
| `test_drizzle_conservation.py` | 守恒映射算子（S7/S8/S9 同批迁移） | `docs/science/drizzle/DRIZZLE.md` §3/§5 |
| `test_area_ratio_l2.py` | 面积比级 L2：**两条负例**，证明该级门在反推路径与切平面分支下**都恒零**、无判别力 | `docs/science/drizzle/DRIZZLE.md` §3.7 / §5.2 |
| `test_healpix_kernel.py` | HEALPix 核与候选枚举（S10/S11，**重建已失的 oracle**） | `docs/science/algorithms/HEALPIX_MAPPING.md` |
| `test_quantization.py` / `test_wcs_plausibility.py` | 量化往返（S12/S13）与 WCS 拒绝组（S5/S5b 同批） | `GAIA_QUERY.md`、`ipv_wcs.cpp` |
| `*_ref.py` | 各用例的**被测参考实现**（按产品源码/正本逐行转写）。它们**不是**期望值来源 | — |

## 三条硬纪律

### 1. 期望值不得来自被测实现自身

期望值只能来自：① 上游正本条款（可逐行核对）；② 闭式/解析推导；
③ 第三方独立实现（`astropy` / `astropy-healpix` / `healpy`，白名单见 `TEST.md` §13）；
④ 定种子蒙特卡洛。
**禁止**用 `*_ref.py` 的输出反推唯一预期值——那只会证明实现等于它自己。
`test_criterion_meta.py::meta.expected_value_source_is_not_the_unit_under_test` 对全层做机器扫描。

### 2. 负例必须真的能红

负例断言的是「**独立判据能抓住这个注入的缺陷**」。若注入后判据仍然通过，
那条负例**无效**——必须改造成有牙齿的版本，不是加豁免。
每条负例用 `harness.evidence()` 把**实测超界读数**记下来，`--verbose` 可逐条核对。

### 3. 条件化不变量不得写成无条件断言

正本里有几条「严格不变量」带着自己的成立条件与反例
（如 DRIZZLE.md 的通量守恒只在**几何闭合**时成立；协方差门只在 `a_p ≥ 0 ∧ c_jp ≥ 0` 时
`exact > diag`，同一节给出带号权重的反例）。这类断言必须写在**条件之内**，
并把反例本身写成正例。

## 被测对象登记（`TEST.md` §13 的要求）

§13 逐字：「每条判据声明真值来源与被测对象集合，且**被测对象集合至少含产品可执行程序本身**」。

| 类别 | 本层的处理 |
|---|---|
| **在库产品实现本体（已对拍）** | `lib/infrastructure/observability/monitoring/runner.py`、`eng/tools/monitoring/run_monitored.py`——纯 Python，无需编译即被 import 并断言 |
| **在库产品源码（逐行转写 + 明确登记）** | `rejection.cpp`、`ipv_wcs.cpp`、`resource_recorder.h`、`resource_gate.h`、`healpix_core.*`、`gaia_client.c`——C++ 侧本阶段**未构建**，故以转写 + 逐行出处登记，**不冒充已执行** |
| **机器正本（直接读）** | `eng/contracts/resource_gate_v1.json`、`eng/packaging/config/defaults.json` |

⚠ 上述 C++ 面按 `TEST.md` §11 的登记纪律记为「**绑定已冻结、执行面未落位**」：
不得在任何判词或台账中把它们记为已执行。接上构建后的对拍由模块/集成层承接。

## 不做的事

- 不裁决代码、不产出阻塞退出码、不进默认构建（构建面 0 个 `add_test`）。
- 不做覆盖门、不做分档、不接 CI（接 CI 的方式由 T12 统一设计，届时也只取 warn）。
- 不堆叠豁免、不用空断言充数、不以「跳过」冒充「通过」。