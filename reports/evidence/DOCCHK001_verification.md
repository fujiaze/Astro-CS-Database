# DOCCHK-001 验证报告 — 机器核对文档函数名/签名/schema/命令/退出码

SHA 基线: 本报告验证时的当前 SHA `bc81298`(SYN-009 登记后 HEAD) + 本任务验证产出。
结论: **PASS**。

## 1. 验收判据(03_TASK_DETAILS.md L132 + PHASE1_API_V1.md §4 API-003 合同)
> 解析 headers/source/schema/help; 核对文档函数名/签名/字段/退出码; **删除/改名/签名 mutation 均使 checker fail**。

doc-symbol-signature checker 合同(PHASE1 §4): 对每个登记函数——① 头文件存在该符号;
② 文档表此行存在;③ 签名(参数数)一致;④ 直接 test ID 非空;⑤ 五字段并发合同齐全。任一缺失 FAIL。

## 2. 交付物
- `tools/check_api_docs.py`: DOCCHK-001 机器一致性检查器(API-003 合同落地)。检查:
  - [A] 命令树: `docs/api/CLI_PROTOCOL_V1.md` §1 每命令与 `build/cli/astrocs --help` 一致。
  - [B] 退出码: 文档列出的 0/2/3/4/5/6/7/8/9/10/70 与唯一源 `cli/exit_codes.h` 定义一致。
  - [C] session 签名(合同③): 每 `p1/p2/p3_session_*` 存在于 session 头文件,参数数(=括号匹配后顶层逗号+1)与文档 §1 一致。
  - [D] 底层函数登记(合同①/②/④/⑤): 每个登记符号存在于真实头文件;§2 表行存在;五字段并发合同齐全;直接 test ID 非空。
  - [E] Phase3 request 字段: 文档 §2 字段映射到 `lib/phase3_session/p3_session.cpp` 消费的 request key。
  - [F] schema 文件存在性: `schemas/*.schema.json` 全部存在且可解析。
- `tests/quality/test_doc_machine_check.py`: 7 用例 mutation 试验证。

## 3. 测试与结果
`python3 -m unittest tests.quality.test_doc_machine_check -v` → **OK**(7 用例)。
干净仓库 `tools/check_api_docs.py` → `{"status":"PASS","failures":0}` rc=0。

| 用例 | mutation | 期望 | 结果 |
|---|---|---|---|
| test_00_clean_repo_passes | 无(干净仓库) | PASS | OK |
| test_01_rename_function_fails | `p1_session_run`→`p1_session_do_the_work` | FAIL | OK |
| test_02_delete_registration_row_fails | 删 §2 `ac_calibrate_frame` 行 | FAIL | OK |
| test_03_signature_change_fails | `p1_session_run` 加参 `int extra` | FAIL | OK |
| test_04_exit_code_remove_fails | `exit_codes.h` 删除退出码 8 定义 | FAIL | OK |
| test_05_command_tree_mutation_fails | `run --phases` 加非法 phase 9 | FAIL | OK |
| test_06_schema_field_mutation_fails | Phase3 §2 `scale_deg_per_px` 改名 | FAIL | OK |

## 4. 扫描发现并修复的文档↔代码漂移
- `docs/api/CLI_PROTOCOL_V1.md` L20: `run --phases <...|2,3...>` 与 `[--cpu-profile]` 改成与 `astrocs --help` 一致的 `<1|2|3|1,2|1,2,3>` / `[--cpu-profile <path>]`。
- `docs/api/PHASE1_API_V1.md` §1: `p1_session_create(const acs_allocator*, ...4 个指针, acs_handle*)` 改成代码真实签名 `p1_session_create(const astrocs_host_services_v1* host, acs_handle*)`(host services 单结构注入)。
- `docs/api/PHASE3_API_V1.md` §1: `p3_session_create` 同样旧 5 指针签名 → `const astrocs_host_services_v1* host, acs_handle* out`。

## 5. 限制
- 命令树核对依赖 `build/cli/astrocs --help` 存在; 若未构建则该检查跳过(不影响其余检查)。
- §2 表用 slash 简写(`ac_generate_master_bias/dark/flat`), 存在性核对取各反引号首段基名; 变体(`_f64/_f32`)经头文件 `_find_param_count` 变体回退核验。
- Phase3 request 字段以代码 `p3_session.cpp` `.value("key")` 为字段权威(该 schema 文件本体未在仓库落地, 故以代码为唯一权威)。
- Windows 侧未在本任务验证(Fatduck 离线, 仅 amd64 Linux, 不阻塞)。

## 6. 独立确认
mutation test 在临时 `docs/api` 副本上做删除/改名/签名/命令/退出码/字段变异, 每项均使 checker rc=1; 干净仓库 rc=0。契约"删除/改名/签名 mutation 均使 checker fail"满足。
