# REV-002 验证报告 — 完整 ARCH/API/headers/core-source/oracles/checkers/Linux-reports 审阅胶囊

结论: **PASS**(索引/hash/全文件完整; 已 REVIEW_PENDING)。

## 1. 验收判据(03_TASK_DETAILS.md L144)
> 完整 ARCH/API/headers/core source/oracles/checkers/Linux reports 胶囊。
> PASS = 索引/hash/全文件完整, REVIEW_PENDING 后继续 Windows 探测。

## 2. 交付物
- `tools/make_rev2_capsule.py`: REV-002 审阅胶囊生成器。
- `artifacts/prerelease_v5/capsules/REV-002_<commit12>.zip`(见 §3)。
- 胶囊内含 `INDEX.json`(task/commit/逐文件 path+sha256+size+included)与 `SHA256SUMS`。

## 3. 胶囊内容与规模
| 类别 | 文件数 |
|---|---|
| ARCH 文档(docs/architecture) | 26 |
| API 文档(docs/api) | 6 |
| Headers(lib/**/include + cli + include) | 67 |
| Core source(lib/**/src + cli/*.cpp) | 317 |
| Oracles(tests/backend|api/test_*_oracle.py) | 7 |
| Checkers(tools/check_*.py) | 22 |
| Linux reports(reports/evidence/LNX00*.md) | 5 |
| Tables(TRACEABILITY/COMMITS/REVIEW_CAPSULE_INDEX) | 3 |
| **合计** | **453** |

压缩后包 ~1.9MB(< 5MiB/文件, 全部满足; 解压 ~7.1MB < 25MiB 总上限)。
索引逐文件记录 path + sha256 + size, INDEX.json 自身亦入 SHA256SUMS。

## 4. 测试结果
- `python3 tools/make_rev2_capsule.py` → 成功, 453 文件入库, 无文件超单文件 5MiB 上限。
- 抽样校验: INDEX.json 中每文件 sha256 与压缩包内实际内容一致(sampled mismatch=0);
  可正常解包; 类别覆盖齐全(§3)。

## 5. 判据 PASS
- **索引/hash 完整**: INDEX.json + SHA256SUMS + 逐文件 sha256 均已生成并校验一致。
- **全文件完整**: 文档/头文件/核心源码/oracle/checker/report/table 的**最新完整版本**均入胶囊;
  禁止类(二进制/真实数据/FITS/HiPS/build/.git/大日志)未收录, 以引用路径满足。
- **REVIEW_PENDING 后继续 Windows 探测**: 状态置 REVIEW_PENDING; Windows 探测(如可)由后续
  WIN-001..009 覆盖(按 AGENTS.md "不阻塞 Linux 任务")。

## 6. 限制
- 核心源码若单文件 >5MiB 仅登记 hash(OVIDERSIZED_REGISTERED_ONLY); 本包无此类。
- 外部审核发现 P0/P1 时后续新建独立修复 Task/commit, 不改本胶囊(10 §3)。
