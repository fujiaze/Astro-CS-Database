# lib

本目录是 ACSD 的生产代码库：科学算法、工程基建与三阶段会话编排都在这里实现。

## 职责边界

- 放：可编译进产品的科学算法（algorithms/）、不定义科学公式的工程基建（infrastructure/）、三阶段装配会话（phase1_session/、phase2_session/、phase3_session/）、公共头（include/）与第三方依赖（third_party/）。
- 不放：机器门与检查器（eng/ci/）、测试套件（eng/tests/）、合同 schema 正本（eng/contracts/）。
- 不放：科学公式与推导正文（docs/science/、docs/science/algorithms/）；本目录只提供对应实现。
- 目录结构按最高设计的顶层结构执行：算法模块并联命名；phase1/2/3 是会话层的内部指代，科学算法目录不用 phase 命名。

## 内容

- algorithms/ —— 科学算法唯一家，子目录并联放置：calibration、cosmetic、star_detection、psf、platesolve、photometry、noise_snr、drizzle、coverage、sampling、upm、rejection、integration、projection、resample、fits_output、shared。
- infrastructure/ —— 工程基建：cli/（normalize/mosaic/export 三个子命令薄入口）、pipeline/（命名块与 typed DAG）、scheduler/（三阶段调度器）、aio/（FITS/HiPS/manifest 唯一 I/O）、benchmark/、observability/、gaia_xpsd_client/、acr/、hips_browser/。
- phase1_session/ —— normalize 阶段装配会话与端口表（p1_session.cpp/.h、module.yaml、memory.md、tests/）。
- phase2_session/ —— mosaic 阶段装配会话（p2_session.cpp/.h、module.yaml、memory.md）。
- phase3_session/ —— export 阶段装配会话与导出接口（p3_session.cpp/.h、p3_v6_export.cpp/.h）。
- include/ —— 对外公共头（astrocs/）。
- third_party/ —— 随仓第三方依赖（nlohmann）。

## 上游

上游：docs/ASTROCS_DESIGN.md §8.4（顶层结构）、§8.5（模块与 ABI）；ENGINEERING_SPEC.md §7（目录规范）。

科学断言以 docs/science/ 为权威，佐证要求见 docs/DOCUMENT_GOVERNANCE.md §2。