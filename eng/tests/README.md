# 独立测试集（`eng/tests/`）

ACSD 独立漏洞检测测试集：主动暴露缺陷、检验科学口径，不作门禁约束代码。
依据：`standards/05_INDEPENDENT_TEST_SUITE.md`；模板见工作包 `TEST_AND_RECTIFY_TEMPLATE.md`。

## 六层

| 层 | 目录 | 内容 | 运行 |
|---|---|---|---|
| unit | `eng/tests/unit/` | 函数与核（含 Oracle 对拍、归零负例） | `python3 -m eng.tests.unit.run_unit` |
| module | `eng/tests/module/` | 单模块输入输出、块读写、生命周期 | `python3 -m pytest eng/tests/module/ -q` |
| integration | `eng/tests/integration/` | 多模块协同、合同/ABI、方差传播 | `python3 -m pytest eng/tests/integration/ -q` |
| pipeline | `eng/tests/pipeline/` | normalize/mosaic/export 阶段内流程（T07 新建） | `python3 -m eng.tests.pipeline.run_pipeline` |
| synthetic | `eng/tests/synthetic/` | 合成全链：五个创新点科学不变量 | `python3 -m eng.tests.synthetic.run_synthetic` |
| e2e | `eng/tests/e2e/` | 真实数据端到端与视觉验收辅助 | `python3 -m pytest eng/tests/e2e/ -q` |

另有 `conformance/`（安装面）与 `validation/release02`（历史验证资产，待前台删除）。

## 每条测试的模板五要素

意图 / 输入（含 seed）/ 预期（含冻结容差）/ Oracle 来源（解析/高精度/Monte Carlo/
第三方独立实现，不以当前程序输出为唯一 expected）/ 负例（注入缺陷，预期归零或报错）。

## T07 交付（RELEASE-10）

- 新建：`eng/tests/pipeline/`（8 文件，69 条：正例 25 / 负例 44）。
- 吸收：T01-D2–D12（D1 登记为 UNRESOLVED 候选 + synthetic 负例臂；D13 不吸收）；
  release02 q1/q2/q3/phot_verify/c_delta/unc/fix_p1/fix_p2b 重写吸收；
  T05 N01–N36（36 条负例，构造待对拍）；T10 十一族（11 锚点，构造待对拍）。
- 性能：`test_perf_probe.py` 六维（CPU/内存/扩展/IO/编排/缓存）。
- 删除：`validation/release02/` 与 `eng/tools/quality` 残留删除交前台统一执行
 （e2e 仍引用 `wcs_lib.py`，quality 生成器仍被 contracts 引用；本任务无 git 写权限）。
- 自验：pipeline 69 条全绿（报告器退出码恒 0；`--exit-code` 自查亦 0 红）。
