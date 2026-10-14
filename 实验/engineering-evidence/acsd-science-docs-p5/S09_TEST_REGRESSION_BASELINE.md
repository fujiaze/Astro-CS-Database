# S09 证据：两处生产补丁的测试集零回归（基线对照）

分支 `audit/acsd-science-docs-p5`。对照方式：把**基线提交** `e31b2be66…` 用
`git worktree add /tmp/acsd-base e31b2be66` 取出，与当前 HEAD 在**同一命令、同一 Python 环境**下
各跑一遍同一个测试子集，逐项比较计数。

```bash
# 基线（worktree /tmp/acsd-base @ e31b2be66）
cd /tmp/acsd-base && python3 -m pytest eng/tests/unit eng/tests/synthetic eng/tests/module \
    eng/tests/integration eng/tests/pipeline -q
# 现状（HEAD，含 P3 drizzle 与 P5 coverage 两处生产补丁）
cd "/workspace/Astro CS Database" && python3 -m pytest eng/tests/unit eng/tests/synthetic \
    eng/tests/module eng/tests/integration eng/tests/pipeline -q
```

## 结果

| 运行点 | failed | passed |
|---|---|---|
| 基线 `e31b2be66`（无补丁） | **29** | **152** |
| HEAD（P3 补丁 + P5 补丁） | **29** | **152** |

⇒ **两处生产补丁造成 0 项测试回归**；差异计数为 0。

聚焦对照（仅两个失败集中的文件）：

| 运行点 | failed | passed |
|---|---|---|
| 基线 `e31b2be66` | 14 | 60 |
| HEAD（含两处补丁） | 14 | 60 |

## 29 项失败的归属（**均为既存缺陷，非本次改动引入**）

- `eng/tests/integration/test_registry_code_parity.py` 与
  `eng/tests/integration/test_variance_propagation.py`：断言前已有 17 条
  `C2/ANCHOR_STALE` 违例，全部指向 **`lib/infrastructure/scheduler/src/module_adapters.cpp`**
  的 `p1_op_*` 函数体锚点 token（如 `/p1_sources.json`、`/calibrated_`、`/p1_phot.json`）
  找不到「代码或字符串」形态的命中。该文件**不在本轮任何补丁的改动面内**
  （本轮 `lib/` 只改了 `drizzle/healpix_drizzle/spherical_overlap.cpp` 与
  `coverage/src/sky_plane.cpp`）。
- 两处补丁的改动面核对：`git diff --name-only e31b2be66 <HEAD> -- lib/` 只含上述两个文件。
- 结论：这 29 项是**基线即有的合同/注册表漂移**，与 P1–P5 科学实验轮无关；本报告只登记，
  不在本任务写面内修复（属 `docs/` 与注册表治理，需另开任务）。

## 归属与限制

- 本证据只声明「**测试集零回归**」，不声明「测试集全绿」。
- `eng/tests/validation/release02/phot_verify/radius_test.py` 在收集期即报错
  （缺 `run/RELEASE-02/L4-rebuild/norm/t2_m1_red/p1_sources.json` 这一历史产物），
  该文件未被本任务创建或删除；故完整 `pytest eng/tests` 会因该既存收集错误中断，
  比较改用上述不含 `validation/release02` 的子集，**两次运行口径一致**。
- 未涵盖：FP32/Windows 构建、`normalize→mosaic→export` 端到端、C++ 单测（仓库无
  `add_test` 注册）。这些一律 `NOT_RUN`。
