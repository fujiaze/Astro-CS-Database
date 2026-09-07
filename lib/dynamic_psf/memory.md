# dynamic_psf - 模块开发memory

> r1（P1-PSF-DOC，2026-09-07）：本文件为 ARCHIVED_NON_NORMATIVE 过程记录；
> 合同权威 = 本目录 README.md（r1）+ module.yaml + docs/algorithms/
> STAR_PSF_ALGORITHMS.md §11。以下历史章节不承载现状判定，参数序等旧描述
> 与实现不符处以 README r1 为准。

## 模块职责
动态PSF拟合，基于Moffat4模型对图像星点进行7参数LM（Levenberg-Marquardt）求解器拟合，输出PSF模型参数供下游测光、匹配、叠加使用。

## 当前版本
- 版本号：v1.1（含性能修复）
- 最新commit：a3ae0d6
- 更新时间：2026-07-12

## GitHub仓库
- 仓库地址：https://github.com/fujiaze/Dynamic-PSF
- 默认分支：master

## 依赖列表
- C++17
- OpenMP（libgomp，16线程并行）

## 关键决策记录
- **Moffat4 PSF模型**：β=4 固定（不可拟合），FWHM=1.230310·σ；7 参数实际序
  B,A,x0,y0,sx,sy,theta（README §5，旧"amplitude/x0/y0/sx/sy/beta/background"序
  描述有误，2026-09-07 勘误）
- **7参数LM求解器**：数值雅可比（前向差分 1e-6 相对/1e-8 绝对步长）+ 高斯消元，
  λ 初值 1e-3，成功/失败 ×0.1/×10；容差 1e-8 / max_iter=200 硬编码
  （DPSFFitParams.maxIter/tolerance 为死参数，DISP-PSF-003）
- **OpenMP 并行**：每星独立拟合 `parallel for schedule(dynamic)` 4 处
  （dpsf_psf.cpp:528,635,738,866），输出按索引写、reduction 仅计数，结果确定

## 进度日志
### 2026-07-12 性能修复（9.26s→0.26s, -97.1%）
- **问题**：单帧36k星点PSF拟合耗时9.26s，全链路45帧需67.5分钟
- **修复措施**：
  1. 日志级别改为WARN（避免DEBUG/INFO大量字符串拼接与IO开销）
  2. 移除双fflush调用（每次写日志强制flush导致syscall风暴）
  3. 添加OpenMP并行（#pragma omp parallel for schedule(dynamic)）
- **结果**：9.26s → 0.26s（-97.1%），16线程并行加速
- 推送至GitHub：commit a3ae0d6

### 2026-07-13 仓库结构整理完成
- GitHub仓库分支统一为main
- 文档刷新并重新推送
- 最新commit: d3ec9e2

### 11.1 PSF 性能优化（2026-07-12）（2026-07-15，从 PROJECT_ARCHITECTURE.md 迁入）

**问题**：PSF 阶段耗时 9.26 s（典型应 < 1 s），根因是 `dpsf_log.cpp` 默认日志级别为 LOG_INFO，每颗星拟合都输出 DEBUG 日志到 stderr + 文件（双 fflush），2000 颗星生成 364 MB 日志文件，I/O 开销主导耗时。

**修复**：
- `dpsf_log.cpp` 默认 threshold 从 LOG_INFO 改为 LOG_WARN
- 移除双 fflush（stderr 和文件均不强制刷盘）
- WARN 及以上级别才写文件（DEBUG/INFO 不写文件）
- `Makefile` 添加 `-fopenmp` 启用 OpenMP 16 线程并行

**结果**：PSF 9.26 s → 0.26 s（**-97.1%**），日志文件 364 MB → 0 KB

## P1-PSF-DOC 冻结（2026-09-07）

- 产物：README.md r1（10 固定章节，全行号锚实测）、module.yaml（CONTRACT_READY，
  schema astrocs.module-manifest/v1）、本文件增补、docs/algorithms/
  STAR_PSF_ALGORITHMS.md §11（SCI-P1-PSF-001/ALG-STARPSF-001/SRC-PSF-001/
  TEST-PSF-DESIGN-001/DISP-PSF-001..006）、docs/contracts/DATA_SEMANTICS.md §15
  （DATA-P1-PSF）、docs/contracts/PUBLIC_API.md API-PSF-001。
- 科学专项落点：known Gaussian/Moffat parameters=README §5/§6（参数序/初值链/
  常量/错误码）；fit failure semantics=4 状态码语义冻结（OK/NO_CONVERGENCE/
  INVALID_PARAMS/ITERATION_LIMIT，README §6）；covariance=现状缺失，登记为
  P1-PSF-IMPL 整改项（DISP-PSF-005）；degenerate/saturated=θ 4 候选消歧 +
  饱和列不消费如实登记（P1-PSF-TEST 专项）。
- 勘误记录：旧 README/memory 的参数序 (amplitude,x0,y0,sx,sy,beta,background)
  有误 → 实际 B,A,x0,y0,sx,sy,theta；β 固定 4；"16 线程"为环境描述非合同值。
