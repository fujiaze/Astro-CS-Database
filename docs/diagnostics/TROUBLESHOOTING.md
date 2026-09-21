# AstroCS Troubleshooting

> 上游：ASTROCS_DESIGN.md §7.2（配置、事件与退出码）

每个 high-risk 错误按固定条目：symptom → likely stage → metrics/log →
error code → minimal reproduction → expected invariant → source/doc/test。

## 故障场景（10 项覆盖门）

| 场景 | likely stage | error/metric | 定位 |
| --- | --- | --- | --- |
| Gaia RSS 暴涨 | P1.STAR/P1.PLATESOLVE | 内存日志、query 计数 | docs/modules/gaia_xpsd_client.md；cache 键审计 |
| Drizzle 极区漏 pixel | P1.DRIZZLE | candidate 计数 | ALG-DRZ-CAND-001 oracle；polar prune 契约 |
| HiPS 黑洞/缺 tile | P1.HIPS_WRITE / P2.HIPS_WRITE | tile 计数、properties | aio_hips_writer；verify CHECKCODE |
| UPM seam | P2.UPM | seam 指标、C 场残差 | SCI-UPM-PERSIST-001 绑定；G4 seam 测试 |
| invalid rejection config | P2.REJECTION | CONFIG error | typed parser；schema 单源检查 |
| NaN 候选权重 | P2.INTEGRATE | INVALID_INPUT 状态 | `p2_validate_candidate_weights`（负/NaN/Inf 拒） |
| corrupt FITS | P1.READ | INPUT_CORRUPT | aio_fits 校验；fuzz/sanitize driver |
| partial output | P1/P2.HIPS_WRITE | IO error + temp 残留 | IO_AND_ATOMICITY；temp+rename 协议 |
| cache mismatch | P2.UPM / Gaia | stale=2 | source_hash/键校验；CACHE_POLICY |
| SNR/ivar 异常 | P1.NOISE / P2.SAMPLER | variance 统计 | NoiseWeightModelV1 矩阵；SNR-001..015 |

## 通用定位顺序

1. 读 stage 日志（run/logs/<module>/<YYYYMMDD>/）；
2. 匹配 error category/code（CONFIG/INPUT_CORRUPT/NUMERIC/IO/...）；
3. 查模块文档（docs/modules/）+ 本表；
4. 复现：最小输入 + 期望不变量（docs/science|algorithms）；
5. 定位源码符号 → 测试（docs/traceability/TRACEABILITY_MATRIX.json）。

## 诊断工具

eng/tools/astrocs_diagnose.py <run_dir> 输出小 bundle（stage/error/metrics）。
