## M2a-C-7 块缓存 4GB 上限在文档按全局声明、实现按每文件私有（MAX_FILES=32），资源验收断言与实现不同径

- 类别: C_DOC_CODE_GAP
- 优先级: P1
- 来源: L10-006
- 位置: docs/algorithms/GAIA_QUERY.md::§7 内存与资源段、::§9 测试门资源行；lib/gaia_xpsd_client/README.md::§7 资源、::§8 测试；lib/gaia_xpsd_client/src/gaia_client.c::BLOCK_CACHE_MAX_MEMORY、::XPSDFileInternal（内嵌 block_cache）、::block_cache_put（判据处）、::MAX_FILES
- 证据摘录（逐字，复核时点现文）:
  > （GAIA_QUERY.md:157）- 内存：mmap 全部 XPSD（只读）+ 块缓存 ≤4GB + 每线程 scratch…；（:232）- **资源**：RSS 结束回落有解释高水位；块缓存 total_memory ≤ 4GB
  > （README.md:107）- 内存：mmap 全部 XPSD 只读 + 解压块缓存 ≤4GB（8192 槽 LRU 1/4 淘汰）…
  > （gaia_client.c:115）#define BLOCK_CACHE_MAX_MEMORY (4ULL * 1024 * 1024 * 1024) /* 解压块缓存最大4GB */
  > （gaia_client.c:434）if (bc->total_memory + data_size > BLOCK_CACHE_MAX_MEMORY || check_memory_pressure()) {   // bc 为 per-file
  > （gaia_client.c:212）BlockCache block_cache;  /* 解压块缓存 (保留到关闭) */（在 XPSDFileInternal 内）；（:30/:222）#define MAX_FILES 32 … XPSDFileInternal files[MAX_FILES];
- 权威依据: 宪章 §10.4（一个进程一个资源预算源）、§10.5（资源记录）、§17.6（无界内存增长属发布门禁禁止项）；TEST-GAIA-DESIGN-001 资源门面（README:127「块缓存 ≤4GB」）
- 问题说明: 判据落在每文件私有 BlockCache 上，client 最多 32 个文件 ⇒ 名义上限 32×4GB=128GB，与文档三处「≤4GB」全局陈述和测试资源门的断言口径不同；唯一兜底是启发式 `check_memory_pressure()`（README:109 自述"可用内存 <4GB 时自适应停止缓存"），不构成预算上限。GaiaDR3+DR3SP 同开即多份缓存并存。
- 影响: 资源验收（§10.5 记录 + §17.6 增长禁止项）与实现不同径，高内存占用不可见；不改单线程数值。
- 建议处置: 二选一并同步验收：实现进程级共享预算（client 级 BlockCache 或全局令牌桶），或合同/README/测试门改述"每文件 ≤4GB、总量按 file_count 上界"并给绝对上限。
- 置信度: 高
- related: M2a-C-8（同模块并发/资源面）、L10-006
