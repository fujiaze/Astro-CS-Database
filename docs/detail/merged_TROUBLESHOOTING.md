# 排障手册（症状 → 定位 → 修复）

> 上游：`docs/ASTROCS_DESIGN.md` §7.2（机器输出与退出码）、§7.3（错误传播与运行日志）、
> `docs/engineering/LOG_AND_ERROR_CONTRACT.md` §5（错误域与退出码映射）、§7（日志面）。
> 写法：`docs/engineering/DOCUMENT_GOVERNANCE.md` §7（写法判据）与 AGENTS.md 第 5 节。

## 1 何时用本文

本手册只覆盖**已被登记的高风险故障**。遇到表里没有的症状，按 §3 的通用定位顺序走，
不要在本表里硬套最像的一行。

每条条目的准入格式是固定的七项，缺项即不算合格条目：

1. 症状（用户看到什么）
2. 阶段（哪个命令 / 哪个节点）
3. 状态码或错误域 / 证据落点
4. 最小复现
5. 期望不变量（复现后应当成立什么）
6. 源码位置（文件与符号）
7. 文档位置与测试位置

## 2 故障场景覆盖门

本表覆盖 10 类高风险场景，每类都有对应的状态码、证据字段与回归测试面。
新增一条故障场景必须同时补上：状态码、正例、负例、登记项。

## 3 通用定位顺序

流程先于索引——先按这五步走一遍，再回到 §4 的表里查具体条目。

1. 读阶段日志。日志落点由块的 `output_dir` 决定，默认 `<output_dir>/logs`；
   落点之外的位置（如仓库 `run/` 下）不写正式日志，不要去那里找。
2. 匹配状态码或错误域（配置 / 输入损坏 / 数值 / IO / 内存 / 取消 / ABI 不匹配 / 退出码）。
3. 查模块文档（`docs/detail/`）与本表。
4. 复现：最小输入 + 期望不变量（正本在 `docs/science/`）。
5. 定位源码符号 → 对应测试（追溯层定义见 `docs/engineering/TRACEABILITY_SPEC.md`）。

## 4 症状主表

按三个命令的顺序排（`normalize` → `mosaic` → `export`），不按内部阶段代号排——
第一读者是操作者，他手里是三个命令。

| 症状 | 阶段（命令/节点） | 状态码 / 错误域 · 证据 | 定位 | 修复动作 |
|---|---|---|---|---|
| orchestrator 启动报 DLL 加载失败 | 全部（启动期） | 动态链接失败 · stderr | Windows 下构建工具链不在 PATH；或三方共用件缺依赖 | 把 MSYS2 mingw64 置入 PATH 后用 `eng/build/toolchain.ps1 check` 自检 |
| `psf` 块不存在 | normalize · PSF | 块缺失 · 阶段日志 | PSF 阶段未运行或已失败 | 查 `stop_after` 是否提前截断；重跑 PSF |
| 校准后全 0 / 无变化 | normalize · CALIBRATE | 数值域 · 阶段日志 | 母版尺寸或数据类型不匹配 | 核对标定母版文件的尺寸与类型 |
| platesolve RMS 异常大 | normalize · PLATESOLVE | 数值域 · 阶段日志 | OBJCTRA/DEC 初值错；SIP 阶数过高 | 修正头信息初值；降低 SIP 阶数 |
| SNR 阶段被跳过 | normalize · NOISE | 跳过状态 · 阶段日志 | 噪声模块未运行或数据块缺失 | 确认噪声模块为当前版本并重跑 |
| 权重全等权（ivar 失效） | normalize · NOISE | 诊断字段 `ivar_product_missing>0` | 输入帧没有 ivar 产品 | 重跑 Phase1 生成含 variance/ivar 的帧；或接受 support 回退 |
| 候选权重为 NaN | mosaic · INTEGRATE | 输入无效状态 | 候选权重含负值/NaN/Inf | 生产侧有校验拒绝；此处说明为何会见到 |
| stage2 权重全为 0 | mosaic · INTEGRATE | 零有效权重 | 帧无 ivar/variance 产品 ⇒ 全部样本权重为 0（`n_accepted>0 ∧ n_positive_weight==0`）。**注意这不是 support=0**——该状态下 support 仍按已接受样本的 max 发布（`docs/science/DATA_SEMANTICS.md` §21.5） | 确认 Phase1 输出了 variance/ivar；重跑 Phase1 |
| 接缝处出现阶跃 | mosaic · UPM | 条件结论 | 接缝压缩只在可表示域内成立，域外结论不成立 | 见 `docs/science/PHASE2_UPM.md` 的适用域与边界声明 |
| 星云带 / 银道面附近误检多 | normalize · 星检测 | 检出域 · 诊断 | 该区域不属星检测方法的标定适用域 | 见 `docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md` 的适用域段 |
| 排异配置被拒 | mosaic · REJECTION | 配置错误 | 归一化组合非法 | percentile 必须配 median_center；rcr 必须为 none |
| HiPS verify 失败 | export · HIPS_WRITE | 产品校验 · 校验日志 | tile 布局或产品缺失 | 用 HiPS 读取器复读；检查 variance/ivar 字段 |
| HiPS 出现黑洞 / 缺 tile | export · HIPS_WRITE | 计数不符 · tile 计数 | 写入未覆盖或校验步失败 | 核对 tile 计数与 `properties` 键值 |
| Drizzle 输出全 NaN | normalize · DRIZZLE | 数值域 · 阶段日志 | WCS/SIP 病态 | 检查 CRVAL/CD/A/B 系数与 pixfrac |
| Drizzle 极区漏 pixel | normalize · DRIZZLE | 候选计数 | 极区剪枝契约 | 见候选 oracle 与极区剪枝契约 |
| corrupt FITS 读入 | normalize · 读取 | 输入损坏 | 文件结构校验失败 | 用 FITS 校验工具复核；见 fuzz/sanitize driver |
| 写出中断 / 部分产品 | export · 写入 | IO 错误 + 临时残留 | 非原子写出 | 见 IO 原子性合同（临时文件 + rename 协议） |
| Gaia 请求量暴涨 | normalize · 星表/天测 | 内存日志 · 查询计数 | 缓存键失效导致重复查询 | 查缓存键审计与缓存策略 |
| cache 命中异常 | mosaic · UPM / Gaia | 陈旧计数 | 源哈希或键不匹配 | 见缓存策略合同 |
| 性能下降 / 超时 | 全部 | 预算错误 · 运行期证据文件 `operation_counts.json`（随产品落盘，不是仓库内文档） | 候选效率异常或线程配置不当 | 检查候选效率与线程配置 |

## 5 常用命令

命令一律以 `ctest --test-dir build` 或仓内实际脚本为准，**不要照抄旧目录结构下的可执行文件路径**——
那些路径在当前仓库并不存在。

```bash
# 环境自检（Windows）
eng/build/toolchain.ps1 check

# 全量构建
eng/build/toolchain.ps1 build

# SNR 科学矩阵（模块级，6 项）
ctest --test-dir build -R "^p1snr_science_" --output-on-failure

# Drizzle 方差传播科学测试（一次覆盖 oracle 幂次 + 产品级正例 + 负例注入，3 项）
ctest --test-dir build -R "^drizzle_pf_sb" --output-on-failure

# Phase2 合成 gate
# 注意：该目标经 gtest_discover_tests 发现期注册，ctest 名带 TEST_PREFIX，形如
#   phase2_synthetic_gate.<Suite>.<Case>
ctest --test-dir build -R "^phase2_synthetic_gate\." --output-on-failure

# 诊断：输出小 bundle（stage/error/metrics）
# 日志目录必须是块的 output_dir 下的 logs，传错目录工具会直接返回 2
python3 eng/tools/astrocs_diagnose.py <output_dir>/logs --json diag.json
```

## 6 外部依赖与网络

- 星表数据集目录（Gaia DR3 及其 SP 表）在本机以只读方式挂载，内容保持原样，不写入。
- 所有外部进程与网络等待必须带 timeout。

---

> 本手册的写法判据见 `docs/engineering/DOCUMENT_GOVERNANCE.md` §7。
