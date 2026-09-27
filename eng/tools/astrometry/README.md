# astrometry

天测精度外部闭环指标的唯一可执行实现与门：检出星经 WCS 前向映射后与星表最近邻的真实大圆角距中位数。

## 职责边界

- 放：闭环指标的 compute / check / fault-inject / selftest 四个子命令，独立于生产实现。
- 不放：口径数值定义正文（在 `docs/science/ASTROMETRY.md` 与 `docs/science/algorithms/GATES_AND_TOLERANCES.md`）、生产 WCS 求解代码（在 `lib/`）、星表锥搜索产物本身。

## 内容

- `closure_metric.py` —— 指标唯一实现：WCS 仅用 astropy 从产物头重建、残差用真大圆角距、同报匹配数与分位数；输入缺失或样本为空一律判红。
- `__pycache__/` —— Python 字节码缓存，不入库。

## 上游

- 注册于 `eng/ci/checks.json`：`CHK-E2E-REPRO` 的步骤 `E2E-WCS-CLOSURE-REPRO-SELFTEST`（`python3 eng/tools/astrometry/closure_metric.py selftest`）。
- 检查项条目见 `docs/ci/01_CHECKS.md`，门禁分级见 `docs/ci/03_GATES.md`。
