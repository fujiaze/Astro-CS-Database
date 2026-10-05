# shared

本目录是算法层的共享实现：被多个算法模块共用的哈希、HEALPix 与精度上下文代码放在这里。

## 职责边界

- 放：跨算法模块复用的基础实现——sha256 哈希、HEALPix 核心、标量与精度上下文公共头、Windows 目录遍历兼容头，以及共址测试。
- 不放：属于单一算法模块的实现（在 lib/algorithms/ 的各并联模块目录）；工程基建（lib/infrastructure/）。
- 不放：科学公式正本（docs/science/、docs/science/algorithms/）；本目录只提供数值实现。

## 内容

- crypto/ —— sha256 哈希实现（sha256.h、sha256.cpp）。
- healpix/ —— HEALPix 核心实现（healpix_core.h、healpix_core.cpp）与第三方声明（THIRD_PARTY_NOTICE.md）。
- include/ —— 共享公共头：astro_scalar.h、precision_context.h。
- dirent_win.h —— Windows 目录遍历兼容头。

## 上游

上游：docs/ACSD_DESIGN.md §8.4（algorithms/shared：共享数学实现）、§8.5（模块与 ABI）。

科学断言以 docs/science/ 为权威，佐证要求见 docs/engineering/governance/DOCUMENT_GOVERNANCE.md「各层准入判据」一节。