# 独立验证树（ACSD v2 重建）

本目录是 `docs/engineering/rebuild/VERIFICATION.md` 要求的独立验证包：每个 case 一个目录，
包含冻结输入、独立真值、被测结果、逐泛函比较和溯源清单。

## 独立性约定

* 真值代码**不** import 生产求解器内部（不 include `src/`、不 import 生产 helper）；
  只用共享数学定义、公开 `include/acsd/contracts.hpp`、公开 schema，以及
  numpy / scipy / astropy(wcslib) / astropy-healpix / healpy 作为外部标准。
* 驱动只搬字段：原生驱动 `validation/native/*.cpp` **只** include `<acsd/contracts.hpp>`，
  自行实现公开的 `Storage` / `Resources` / `Cancellation` 三个服务接口，不含任何科学计算。
* 输入与请求先冻结并 hash，再运行被测程序；失败的 attempt 追加到
  `observed/attempts/`，不覆盖。
* 真值类型在 `case.json` 的 `truth_type` 中标注：
  `analytic_exact`、`deterministic_enclosure`、`stochastic_confidence`、`empirical`、
  `conditional_model`。
* 被测入口尚未实现时记 `pending_implementation` 并写明原因，**不伪造结果**。

### 比较器红线（比较器本身必须可证伪）

1. **每个泛函都要记成功计数**：写明 `expected_success_count` 与
   `observed_success_count`。成功数为 0 而期望数 > 0 → 判 fail 并写明
   `expected_success_count>0, observed_success_count=0`。
2. **禁止「无样本即 0」**：任何「取成功样本的最大值」的写法在成功集为空时必须判
   fail，绝不能用 0.0 之类的默认值充当 observed（这是 2026-10 一次真实漏判：
   C01 的 inverse 全部失败却被判 pass）。
3. **必须有 case 级覆盖检查**：每个 case 至少有一个真值样本被成功比较；
   否则该 case 判 fail，不得因为「无样本」而静默通过。
4. **失败直方图**：每个量测族记录 attempts / successes / 具名失败直方图
   （code + reason 计数），失败不得被丢弃或折叠成一个 0。
5. **域内/域外必须区分**：目标落在声明域外时，期望是「具名失败」；域内目标必须
   成功。判定用独立于被测实现的方式（外部参考或自写闭式模型），不得用被测实现
   自己的输出来决定期望。
6. **反例不得伪 PASS**：预算耗尽（work/precision/depth）时若仍报
   `termination=tolerance_met` 且 `bound_validity=certified`，判 fail。

## 目录

```
validation/
  run_all.py            一键运行全部 case 并汇总
  lib/                  共用工具（IO、精确有理数、驱动、可用性检查）
  native/               只 include 公开合同的内存 API 驱动
  c01/ g01/ u01/ f01/ s01/ w01/ v01/ e01/ p01/ r01/ x01/
      case.json         目标/假设/估计量/单位/condition/容差/正负例预期
      input/            解析后的精确数值模型、块与 hash
      truth/            独立算法与其结果
      request.json      冻结的合同驱动请求
      observed/         驱动原始响应、stdout/stderr、资源账本
      comparison.json   逐泛函比较与失败作用域
      manifest.json     来源、版本、seed、trial 分母、代码/库身份、依赖与命令
```

## 运行

```bash
python3 validation/run_all.py                 # 全部
python3 validation/run_all.py c01 u01         # 指定 case
bash validation/lib/build_native.sh u01_driver c01_driver   # 只编译原生驱动
```

`run_all.py` 会先编译原生驱动（某个生产模块暂时编译不过时，只影响依赖它的 case），
再逐个执行 case 脚本，最后写出 `validation/run_summary.json`。

## case 一览

| case | 内容 | 真值类型 | 当前状态 |
|---|---|---|---|
| C01 | WCS 绝对坐标：forward / inverse / 导数 / g，原点与 CD 负例 | analytic_exact | **实跑 69/69 通过**（含 SIP 2..9 阶与 8 个负例） |
| G01 | 真实面积：解析球面超额、自研细分包围、MC | analytic_exact + enclosure + MC | **实跑 171/171 通过**（面积、逐叶之和、work/precision 负例） |
| U01 | 同 R 矩阵、协方差、符号孔径方差、拆同源负例 | analytic_exact | **实跑 14/14 通过**（内存 API + 合同进程双路由） |
| F01 | 像元积分测光、共享 latent、秩负例、尺度区间 | analytic_exact | 观察侧已实跑；PSF 形状拟合在我的夹具上判秩亏（见 comparison.json） |
| S01 | 相对天光：KH=0、交叉项、nullspace | analytic_exact + MC | 真值就绪；观察侧待实现 |
| W01 | BLUE / PSD / 非负 QP 与 100000 次全流程 MC | analytic_exact + MC | 真值就绪；观察侧待实现 |
| V01 | 稀疏场景方差：δ/M 界、洞、C∞ bump 反例 | analytic_exact + conditional | 真值就绪；观察侧待实现 |
| E01 | Export：支持均值/积分/外推、同叶合并、FITS 头 | analytic_exact | 真值就绪；观察侧待实现 |
| P01 | 产品闭包：缺块、hash 冲突、重复曝光、未知 recipe | conditional_model | 真值就绪；观察侧待实现 |
| R01 | 资源与可复现：线程/内存/cache/取消/乱序矩阵 | conditional + empirical | 真值就绪；观察侧待实现 |
| X01 | 端到端：物理 Iλ → 原始 ADU → 三命令、盲解 | empirical + analytic_exact | 三命令已实跑；A 组推进到 PSF，B 组盲解未过（见 observed/e2e_summary.json） |

详细数字见各 case 的 `truth/`、`observed/` 与 `comparison.json`。
