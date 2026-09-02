# 科学文档、合成 Oracle 与独立审核合同

## 1. 六层单向推导

正式资产必须按以下方向形成，代码不能反向定义科学含义：

`SCI 定义 -> ALG 离散算法 -> ARCH 数据流/并发 -> API/ABI -> CODE -> TEST/ORACLE`

每个核心 claim 分配稳定 ID，并在 `TRACEABILITY.csv` 形成闭环：文档标题/公式/单位/假设 -> 算法步骤/误差 -> 接口符号/字段 -> 源 symbol -> test/oracle。

## 2. 科学文档最低内容

Calibration、WCS/PSF/Photometry、Noise/SNR、Drizzle、UPM、Rejection/Integration、Phase3 HiPS→平面 FITS 均须写：

- 数学对象、域、坐标系和物理意义；
- 输入输出、单位、shape、mask/NaN/Inf 规则；
- 方程、假设、边界、退化情形；
- uncertainty/variance/weight 的严格定义与传播；
- 数值精度和允许容差的来源；
- primary literature：作者、题名、年份、DOI/ADS/arXiv、具体 section/equation；
- 项目原创部分明确标为 project-defined，给出完整推导，不伪造论文背书。

核心单位禁止“ADU 或 ADU/pixel”这类二选一表述；必须冻结一种定义并说明换算。

## 3. 算法与工程文档

每个 ALG 写连续公式到离散步骤、复杂度、数值稳定性、并行可交换性/归约误差。ARCH/API 文档逐函数写：完整名称、签名、每个参数、单位、所有权、生命周期、错误、线程安全、可重入、同步/异步、内部并行、取消点、确定性和调用者。

文档中的函数名和字段必须由脚本从源码/headers/schema 核对；不存在、改名或签名漂移直接 FAIL。

## 4. 合成验证目录合同

每组固定结构：

```text
tools/validation/<group>/
  contract.json
  generate.py
  oracle.py
  run.py
  README.md
```

`oracle.py` 必须独立表达科学公式，禁止调用生产 kernel 或复制生产实现。`contract.json` 在运行前冻结 seed、输入域、绝对/相对/ULP 容差、统计检验和失败判据。

组：`calibration`、`wcs_psf_photometry`、`noise_snr`、`drizzle`、`upm`、`rejection_integration`、`pipeline_cli`。

每组至少覆盖：解析解、随机固定种子、边界/退化、NaN/Inf/mask、尺度与单位、FP32/FP64、1 worker 与 N worker、baseline 与每个 ISA backend。并行非确定归约可用预冻结误差合同，不得要求 bitwise；科学不变量必须保持。

## 5. 禁止历史束缚

- 不运行旧二进制、不对比旧 HiPS、不把旧结果作为 oracle。
- V3/V4 报告只用于列出待复核风险。
- 如需要数值实现互检，使用独立 scalar reference + 合成数据，不使用历史生产版本。

## 6. 外部科学审核

重要 SCI/ALG/ARCH/API 与对应核心实现形成 review capsule。外部审核者负责：

- 100% 审阅核心科学/算法/架构/API、公式、ABI、autotune、资源门禁和独立 Oracle；
- 核对 primary literature 的原文 equation/section 与项目推导；
- 对非核心源码按最终 commit SHA 做确定性 20% 文件抽样；
- 抽查运行合成 Oracle，不接受 Agent 自报 PASS。

审阅异步进行：胶囊标 `REVIEW_PENDING` 后，Agent 继续不依赖审阅结论的 Task。只有最终发布审核是强制停止点。
