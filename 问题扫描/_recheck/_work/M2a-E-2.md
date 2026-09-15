## M2a-E-2 GAIA 合同链的源码行锚系统性漂移（+120~170 行），"逐函数核对/与源码一致"声明因此不可复核

- 类别: E_TRACE_BREAK
- 优先级: P1
- 来源: L10-005
- 位置: docs/algorithms/GAIA_QUERY.md::§2 各小节头（角距/unproject/bbox/polar 剪枝/常量表）、::§4 互斥行；lib/gaia_xpsd_client/src/gaia_client.h::文件头锚清单；lib/gaia_xpsd_client/README.md::§9 最后核对行；tests/unit/gaia_xpsd_fixture_gen.c::文件头；对照 lib/gaia_xpsd_client/src/gaia_client.c 现符号位置
- 证据摘录（逐字，复核时点现文 vs 当下树实测）:
  > （GAIA_QUERY.md:63）"unproject gaia_client.c:622-646" ｜ 当下树 `static void unproject` 实存 **gaia_client.c:750**（漂移 +128）
  > （GAIA_QUERY.md:197）"命中路径在持锁状态下完成 O(N) 输出构造（gaia_client.c:1658-1684）" ｜ 现命中装配在同函数区间的 ~:1829 附近（含 :1829 calloc 注释），漂移 +170 量级
  > （GAIA_QUERY.md:147）"结果上限 … MAX_STARS_RESULT | 源码 27, 1146, 1215" ｜ 当下树 `#define MAX_FILES 32` 在 :30 区、`MAX_STARS_RESULT` define 不在 :27（:27 是 WL_COUNT 面）
  > （gaia_client.h:1-18 头注）"…gaia_client.c:112-128 (QueryCacheEntry) / 427-472 (query_cache_lookup) / 74-75 (GAIA_CACHE_VERSION)" ｜ 当下树 QueryCacheEntry 在 :159 区、collect_plan_stats 在 :237、缓存版本宏在 :121 区
  > （README.md:148-149）"最后核对基线：CAT-GAIA-DOC 于源码 2255 行逐函数核对（run/local/agent_cat_gaia_doc/）" ｜ 当下文件 2441 行
- 权威依据: 宪章 §12.3-1/-2（符号追踪无断链、活动文档禁陈旧状态）；规程 §2（锚点漂移属 P1 典型）
- 问题说明: 行锚集体停留在迁移前的旧版本源码（漂移 +120~170 行），而合同头部同时自述"全部离散公式、常量、行为边界均从该文件逐函数核对"（GAIA_QUERY.md:6）——自述的可复核性现在不成立。头文件里的锚清单同样过期，读者按头注跳转会落到无关代码（例如 QueryCacheEntry → 实际是别的结构）。本轮复核已按符号重定位，**未因行号不符拒绝任何条目**。
- 影响: 追溯链不可达（不影响数值）；后续代理/审计按锚回验会整体落空，是本轮多起"锚点找不到→误判"的共同成因。
- 建议处置: ALG-GAIA-001/头文件/README 的行锚一次性改为 `gaia_client.c::<符号>` 形态（函数附区间由工具生成），并把"ALG 行锚可解析性"并入追溯机器检查（与 M2a-E-1 建议③同批）。
- 置信度: 高
- related: M2a-C-5、M2a-E-1、M2a-E-4；本轮"行号漂移处置"总述见 _merge/M2a.md 第二节
