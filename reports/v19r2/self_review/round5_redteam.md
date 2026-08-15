# Round5 — 独立红队（§18 20+ 假设）

按 20 条攻击面逐条核查：

1. 文档迎合代码？— authority 优先（science > contract > docs > tests > code），
   DATA_SEMANTICS 补 variance/ivar 是真缺口修复（F-V19R2-DOCS-001）。
2. stale comment 仍说骨架/Vxx？— comment hygiene 0 violation（434 文件）。
3. 无 owner doc 的源文件？— 所有 shipping 模块有 docs/modules/*。
4. shipping 漏审计？— 713/713，0 UNREVIEWED。
5. test 错算 shipping？— shipping=280 仅生产单元，tests 独立分类。
6. warning 冒充 static？— -fanalyzer 独立运行 5 单元 0 finding。
7. sanitizer 只跑一个模块却写全量？— 如实标注 V18 WSL 证据 + exception。
8. raw pointer ownership 不明？— C API 契约文档化（ENG-OWN-001）。
9. cache 线程不安全？— dense cache/gaia 缓存键+失效+线程模型文档化。
10. rc/status 双语义？— F-V19R2-UPM-002 修复后扫描 0 命中。
11. old SNR 影响 production？— legacy SNR 仅诊断（SNR-008 退休）。
12. Drizzle fast path 无 reference？— candidate/overlap oracle 9003+77。
13. duplicate science path 藏在 tool？— 扫描 0。
14. config default 两处不一致？— 一致性工具 6/6 + schema 单源。
15. docs enum 与 header 不一致？— AIO product flags 对照 PASS。
16. error code 无 troubleshooting？— 10 场景表 + ERR-* 契约。
17. performance comment 过时？— 本轮注释清洗含 perf 语义保留。
18. history 注释污染源码？— 已迁移（1240 处）。
19. archive 被 build？— healpix_stack 冻结跳过；archive 不参与构建（确认）。
20. source 包混 build/vendor？— S10 打包清单排除 build/vendor/data。

结论：PASS（20/20）。
