# WIN-VERIFY-001 — Windows 正式节点复验包（索引）

- 控制包：`AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915`（Wave 11）
- 基线 HEAD：`8f5ef3e9c84590895ebaadbb30c0e6a7ab72d467`（只读 `git rev-parse` 复核）
- 产品版本：`0.11.0-alpha.2`（根 `VERSION`，未改动）
- **结论标注：AWAITING_WINDOWS_VALIDATION**
- write_scope：`artifacts/v6/windows/`、`reports/v6/windows/`（未越界）

## 结论摘要

| 项 | 结果 |
|---|---|
| Fatduck 可达性 | **不可达**（ssh Connection timed out / TCP 22 超时 / ICMP 100% 丢包 / Tailscale offline, last seen 3h ago） |
| 同 SHA Windows 候选 | **不存在**（Windows CI run 35012779853 failure，Upload candidate skipped） |
| Windows 复验用例执行 | **0/32 executed**（全部 UNAVAILABLE） |
| 可否宣布发布 | **否**（§17.12 门 7/8 均未满足；宪章 §15.3 明令不得据此发布） |

## 文件导航

| 路径 | 内容 |
|---|---|
| `candidate/CANDIDATE_MANIFEST.json` | 候选身份、期望成员、缺失原因、ABI/工具链期望 |
| `candidate/CANDIDATE_SHA256.csv` | 16 成员 + zip 的 sha256 清单（**当前全 UNAVAILABLE**，附 Windows 侧填充命令） |
| `candidate/CANDIDATE_PROVENANCE_CHAIN.json` | GH Actions run/artifact/job 证据链（含真实 artifact digest） |
| `GITHUB_ACTIONS_EVIDENCE.json` | 基线 SHA 的 Windows/Linux/Fatduck run 与 job 明细、无认证 API 方法 |
| `FATDUCK_REACHABILITY.json` | 5 条可达性实测命令/退出码/输出 |
| `checks/win_verify_cases.json` | 机器可读 32 用例（判定清单，含负向门） |
| `checks/VERIFICATION_CHECKLIST.md` | 人读验证步骤与 PASS/FAIL 判据、复现步骤 |
| `checks/run_windows_verification.ps1` | 自包含参考 harness（未执行，待节点部署与 parse 校验） |
| `UNVERIFIED_ITEMS.md` / `.json` | 未验证项 + 须在 Windows 节点执行的清单 |
| `evidence/EVIDENCE_INDEX.json` | 本包全部文件的 sha256 与来源 |
| `logs/` | 全部命令的真实日志（可达性、GitHub API 响应） |
| `../../reports/v6/windows/WIN-VERIFY-001.md` | 正式报告 |

## 交付纪律声明

本包未 commit/push，未做任何 git 写操作，未派生子代理，未宣布发布，未修改生产源码/测试/docs/contracts/ci，未读取或复制 SSH 私钥内容，未编造任何 Windows 侧结果或 sha256。
