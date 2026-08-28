# Round 5 — Red-Team（V18R2，15 攻击假设）

| # | 假设 | 结论 | 证据 |
| --- | --- | --- | --- |
| 1 | profiler 造成加速假象 | DISPROVED | fine profiler 默认关闭（ASTROCS_DRIZZLE_FINE_PROFILE 门控）；before/after 均用同构剖析采样；最终 16 帧 67.35s 与单帧 65.7s 一致 |
| 2 | fixed geometry fast path 漏 candidate | DISPROVED | candidate oracle 9003/9003 PASS；scratch 复用后 t_drop_geom clip_normals 未清空 bug 已修（先 FAIL 后 PASS） |
| 3 | boundary array 与 vector oracle 不一致 | DISPROVED | get_healpix_boundary4 与 get_healpix_boundary 同一 4 角计算；edge/overlap oracle PASS |
| 4 | acos→dot 改边界判定 | DISPROVED（第一版 BUG 已修） | 1e-19 sliver oracle 证明 dot<cos(lim) 不等价 → 安全余量方案（lim+1e-9）拒绝集为原集严格子集；edge oracle PASS |
| 5 | hierarchy 优化改变 parent 数值 | DISPROVED | NESTED sig/sup 缓存与 FITS 序读回逐位一致（同一 float/double 存储）；单帧 signal max_abs 5.6e-9（FP32 级） |
| 6 | FP32 merge order 改变 science | DISPROVED | 单帧对比 V17 产物：signal 5.6e-9、support 4.6e-6（FP32 舍入级） |
| 7 | SNR malloc leak | DISPROVED（已修） | RAII vector 替代 malloc/free；HiPS-only 路径无泄漏（原 free 仅在 legacy 路径） |
| 8 | HiPS scratch 跨 tile 未清 | DISPROVED | ProductSet scratch 每 tile 重填（sig_n/sup_n resize+写）；单帧 tile 57/57 与 V17 同构 |
| 9 | global omp thread side effect | DISPROVED（已修） | omp_set_num_threads → parallel num_threads 子句；calibration 显式接口保留 |
| 10 | data_pipeline 实际仍 build | DISPROVED（已删） | git rm；rg 无构建引用；astro_image_io canonical |
| 11 | SHA 实现仍复制 | DISPROVED（已归一化） | orchestrator/ACR 3 份 SHA256 删除，统一 common/crypto；配置 SHA 一致 |
| 12 | config typed/JSON 双路径 | DISPROVED（V17 已清理） | stage1/stage2 typed 单一解析；legacy aliases 已删（V17） |
| 13 | performance subset 冒充 full | DISPROVED | 最终验证 = 1 次完整 16 帧（非 subset），16/16 rc=0 |
| 14 | wall accounting 重叠计时冒充 98% | DISPROVED | 资源剖析按日志绝对时间戳对齐阶段（非累计）；fine profile 累计时间标注线程累计 |
| 15 | docs 仍写旧 Stage2/Phase1 状态 | DISPROVED（已修） | HANDOVER 重写为 V18R2；legacy Stage2 移除（V17）已如实 |

```text
ROUND5=PASS（15/15 闭环；另抓出 clip_normals 复用、spectrum 剪枝、polar 符号 3 个实现 bug 并修复）
```
