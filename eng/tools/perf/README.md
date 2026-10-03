# eng/tools/perf/run_perf_synth_chain.py —— 小批量**合成数据**全流程性能驱动

> 上游：\`docs/ACSD_DESIGN.md\` §8.3/§9、
> \`docs/detail/infrastructure/21_observability.md\` §8（G-RES-01 语义权威）、
> \`eng/contracts/resource_gate_v1.json\`（**唯一**阈值数值源）。

## 1. 它是什么 / 不是什么

**是**：把「注册合成场景 → 三命令串行（normalize/mosaic/export）→ 既有探针采样 → 冻结判据裁决」
接成一条可复跑、出机器可读证据的链路，并在每个阶段落 \`run_monitored\` 的 JSON/CSV 读数。

**不是**：不新造任何埋点；不复制 \`eng/tools/monitoring/\` 的采样与判定实现；不发明阈值；
不做科学判据（科学保真度判据在 \`实验/additive-sky-seamless/.../run_reconstruct.py\`，本工具不越界）。

## 2. 为什么必须新建（任务书第 1 条的判定依据）

规范路径的三命令串行驱动是 \`eng/tools/e2e/run_e2e_chain.py\`，但它**只吃真实帧**——配置由 \`eng/tools/e2e/make_e2e_configs.py\`
从 \`testdata/M42_T2T3_mosaic_Flying_dutchman/\`、\`testdata/Galaxy_Center_T4/\` 构造，
含写死的 RA/Dec 中心。合成数据侧现有三件都不能独立驱动全链：

| 现有件 | 能做的 | 缺口 |
|---|---|---|
| \`实验/shared/data/synthetic/generate.py\` | 按 \`datasets.json\` 生成 DATA-TYPE-MATRIX 数据 | 解析玩具仪器产物：无 TAN WCS、无校准母版 ⇒ normalize 的 WCS/定标链不可用 |
| \`实验/shared/synthetic/m16_sampling.py\` | 真实信号模板 → 仿真采样帧 + **与探测器模型一致的合成母版** + 真值画布（TAN WCS 一等公民） | 只到「生成数据」为止；不构造三命令配置、不驱动 CLI、不落资源证据 |
| \`实验/additive-sky-seamless/code/reverse_verify/m16_sampling/run_reconstruct.py\` | 仿真帧 → normalize → mosaic → export → 与真值比对 | 判据是**科学保真度**（互相关峰位/结构残差/测光）；不采样资源、不判 G-RES-01；二进制路径写死已退役的 \`build/acsd\` |

⇒ 规范路径**缺一个合成数据驱动件**。本工具即该接线层：数据生成调 \`m16_sampling\`（唯一事实源），
三命令串行与输出目录纪律沿用 \`run_e2e_chain\` 的口径，采样与判定调既有预埋面。

## 3. 判据面（两层，均 fail-closed，均不改判据）

| 层 | 实现 | 判据 | 语义 |
|---|---|---|---|
| ① 生产侧 G-RES-01 | \`eng/tools/monitoring/run_monitored.py::evaluate_frozen_gate\` | 硬失败：单活跃计算线程、低利用窗且队列有工作、无界内存增长；记录项：平均/p50/逐样本占比 | 阈值读 \`eng/contracts/resource_gate_v1.json\`；不适用（有效核<2 或区间 ≤10 s）为**显式分类** |

> 现态说明：原第二层「CI 裁决面 L2」依赖已退场的门禁脚本与门禁规范篇，该层裁决面已下线；
> 本工具只保留生产侧 G-RES-01 一层判据。

外加本工具自有的两条**记录面**（不产生退出码，但越界即红并写进报告）：
内存峰值 ≤ 声明预算（取实测正常峰值的 1.5–2 倍）、I/O 等待占比与写量（口径见 \`21_observability.md\` §8.7）。

## 4. 受控编排键（**只有这些**进命令面）

ACSD 的并发由调度器从「Runtime lease」与「内存闸门」派生；phase 配置**禁止**携带
workers/ISA/block（\`eng/contracts/schemas/phase_config_*.schema.json\` 的硬件字段禁令）。因此：

| 键 | 作用 | 依据 |
|---|---|---|
| \`--cpuset <mask>\` | \`taskset -c\` 改 lease（帧级并发的唯一来源） | \`docs/ACSD_DESIGN.md\` §9 可用 CPU = 亲和性 ∩ cgroup |
| \`--axis-frame\` / \`--axis-inner\` | \`ACSD_P1_AXIS_FRAME_WORKERS\` / \`ACSD_P1_AXIS_INNER_OMP\`：帧级并发 × 帧内并行两轴分配 | \`module_adapters.cpp\` 标定旋钮；越界（\`frame_w × inner_u > budget\`）被拒绝并留痕 ⇒ 总并行度 ≤ lease 不破 |
| \`--export-scale-arcsec\` | export 输出像素尺度（几何由真值画布 WCS 推出） | 输出画幅是**测量配置**，不是判据 |
| \`--scale\` | 运行期派生场景倍率（帧尺寸与指向网格步长同乘 k） | 工作量标定；\`k=1\` 与注册件逐字段相同（self-test S6/S7 断言） |

**没有**任何形式的工作线程硬编码、ISA 选择或 block 尺寸参数。

## 5. 用法

\`\`\`bash
# 判据能红能绿自证（不触碰真实重计算）
python3 eng/tools/perf/run_perf_synth_chain.py selftest

# 开测前置检查（协议 1）：判据 = **掩码内** 3 s 瞬时忙碌核，不是 loadavg
python3 eng/tools/perf/run_perf_synth_chain.py precheck \
  --max-busy 4.0 --max-load 8.0 --cpuset 0-7 --interval 60 --tries 30

# 一次受监控全链（重计算必须套 mem_guard —— 本工具已在内部套）
python3 eng/tools/perf/run_perf_synth_chain.py run --tag base \
  --scale 1 --workers 8 --cpuset 0-7 --binary /path/to/self-built/acsd \
  --export-scale-arcsec 4.0 --max-rss-gb 8 --mem-budget-gb 4 --timeout 1800 \
  --gate-required --skip-generate-if-present --keep-dataset

# 受控 A/B：唯一变量 = 并行轴形态（PERF-760 实测缺省策略值 = 8 帧宽 × 2 内宽）
python3 eng/tools/perf/run_perf_synth_chain.py run --tag axis-1x8 --axis-frame 1 --axis-inner 8 ...
python3 eng/tools/perf/run_perf_synth_chain.py run --tag axis-2x4 --axis-frame 2 --axis-inner 4 ...

# 负例（判据必须能红）：把整条链钉到 1 个 CPU ⇒ lease 退化 ⇒ 判据①与 L2 四条都应判红
python3 eng/tools/perf/run_perf_synth_chain.py run --tag neg-lease1 --workers 1 --cpuset 0 ...

# 样本有效性（隔离协议 3；主口径 = **掩码内**外来份额）
python3 eng/tools/perf/run_perf_synth_chain.py validity --tags base axis-1x8 axis-2x4 neg-lease1

# 前后对照 + 报告表体 + 汇总判词
python3 eng/tools/perf/run_perf_synth_chain.py set    --tags base axis-1x8 axis-2x4 neg-lease1
python3 eng/tools/perf/run_perf_synth_chain.py report --tags base axis-1x8 axis-2x4 neg-lease1
#   报告末尾打印 PERF760_VERDICT = 通过 / 未通过（L2 有红行即未通过）
\`\`\`

退出码：\`0\` 全绿（含判据）；\`1\` 有判据判红；\`2\` 用法/锚/环境错误。

## 6. 产物落点（\`--run-dir\`，缺省 \`run/PERF-760\`）

\`\`\`
evidence/<tag>_conditions.json      测量条件（机器/亲和性/内存、binary sha256、cpu_profile 摘要、df）
evidence/<tag>_<stage>_monitor.json run_monitored 原始读数（外部采样 + frozen_gate）
evidence/<tag>_<stage>_timeseries.csv 逐样本 CPU/RSS/线程/就绪线程/IO（探针读数落 CSV）
evidence/<tag>_<stage>_worker_balance.csv  CLI 自报 worker 均衡（含已知退化标注）
evidence/<tag>_chain.json           全链汇总：条件 + 各阶段 + 判据裁决 + I/O + 均衡 + 几何
evidence/compare.json               set 子命令的前后对照
waterfall/<tag>_<stage>/node_waterfall.{md,json}  节点级瀑布（探针归因）
stages/<tag>/<stage>/mem_guard.json mem_guard 峰值 RSS 留证
dataset/<set>/                      合成数据集（可复跑，含 perft760_generate.json）
out/<tag>/{norm,mosaic,export}/     三命令 CLI 运行产物（可再生）
\`\`\`

## 7. 已知退化（如实登记，不掩盖）

- **\`worker_balance.csv\` 恒 50.00%**：\`lib/infrastructure/cli/commands.cpp\` 以
  \`recorder.set_workers(budget, budget)\` 写入 active/runnable 两列 ⇒ \`utilization_pct\` 不随真实负载变化
  （见 \`实验/engineering-evidence/l2_performance/README.md\` §7）。本工具改为同时给出外挂监控的
  \`runnable_p50\` / \`cpu_percent_p50\` 作为**权威口径**，并在 JSON 里带 \`degenerate_note\`。
- **节点的 \`inner_omp\` 是「每帧内宽」不是「总线程数」**：\`in_flight × inner_u ≤ budget\` 是硬不变式；
  \`n_units < frame_w\` 时总宽度 = \`n_units × inner_u\`（帧数不足 ⇒ 吃不满预算，属编排事实而非缺陷）。
- **\`--scale\` 派生场景改变覆盖天区**：\`k>1\` 时指向网格步长同比放大，逐帧科学语义（像素尺度/PSF/天光/
  曝光/探测器模型）不变，但覆盖天区更大。报告须显式声明该边界。
