# AstroCS CLI 协议合同 v1 (API-002 冻结 — 权威=控制包 04)

> ID: API-CLI-001  状态: FROZEN (V5 API-002, 2026-08-28)  上游: API-001/ARCH-002  下游: CLI-001/002/003, API-003..005(handler 追溯), BENCH-005
> 本文件为控制包 04 的仓库落地;两者冲突以 04 为准并在 traceability 登记。
> 命令树已于 CLI-001 切换为 ASTROCS_DESIGN §6.1/§6.2 的**唯一命令树**：用户命令只有
> normalize/mosaic/export + help/--version/doctor/benchmark；旧 phase1|2|3 用户命令与
> 别名（含 config */modules */selftest/test synthetic/verify*/drizzle/benchmark cpu|
> verify-profile/hardware inspect）**全部删除且 rc=2**（phase 仅为内部指代，§6.2）。

## 1 命令树(ASTROCS_DESIGN §6.2 唯一命令树;help 文本 golden 由此生成)

```text
astrocs --version [--json]
astrocs normalize (--json <config.json> | --template [-o <path>] | --help)
astrocs mosaic (--json <config.json> | --template [-o <path>] | --help)
astrocs export (--json <config.json> | --template [-o <path>] | --help)
astrocs help
astrocs doctor [--json]
astrocs benchmark
```

命令语义（§6.1 薄入口）：
- `--json <config.json>` 运行：运行前预检（绿/橘/红）→ 存在 error 强制阻断（仅 `-force`
  可越过可强制项）→ 用户输入 `yes` 确认（`-y` 跳过）→ 执行并落产品 + manifest；
- `--template [-o <path>]` 生成可直接改的完整 JSON 模板（缺 `-o` → stdout）；
- `--help` 子命令帮助与字段说明；
- 三个命令**平级独立**：各自独立进程、独立恢复、独立验收，**禁止**隐式串接（§1.2）；
- `benchmark` 生成/更新**安装目录** cpu_profile（后续运行自动读取）。

handler→内部会话 API 追溯(04 §6-4,phase 为内部指代): normalize→API-003(会话1)；mosaic→API-004(会话2)；export→API-005(会话3)；benchmark→BENCH-001..004 harness(内部)。

## 2 退出码(04 §2 全 11 条冻结,唯一源 `include/astrocs/exit_codes.h`)

0 成功且门禁全过 / 2 CLI 参数或配置错 / 3 输入缺失格式 hash 错 / 4 科学验证或不变量失败 / 5 backend ABI 签名 CPU 特征或加载失败 / 6 计算执行失败 / 7 I/O 失败 / 8 输出完整性验证失败 / 9 用户取消或超时 / 10 资源利用率或内存增长门禁失败 / 70 未分类内部错误(必须出脱敏 crash report)。跨平台同失败同码(golden 双平台断言)。

## 3 stdout/stderr 纪律(04 §3)

- 人类模式: stdout=简洁结果, stderr=日志/诊断;`--json`: stdout 恰一个 JSON 文档;`--events-jsonl`: stdout 每行一个 UTF-8 JSON 事件,禁夹普通文字;JSON 路径全 UTF-8(Windows 内部 Unicode 路径正确处理)。
- **stdout 无日志污染**为机器测试项(CLI-002 golden)。

## 4 JSONL 事件 v1(04 §4 字段冻结)

- 每行必含: `schema_version,event_id,run_id,timestamp_utc,sequence,kind,severity,phase,stage,message`;`sequence` 从 0 单调递增。
- kind 扩展字段: progress{completed,total,unit,rate,eta_seconds} / resource{cpu_cores_used,rss_bytes,io_read_bytes,io_write_bytes,threads} / artifact{role,path,sha256,size_bytes} / backend{kernel,backend_id,isa,workers,block_size,reason} / final{exit_code,status,run_manifest,summary}。
- 重计算 stage 必发 `stage_start/stage_end`+实际 backend 事件;GUI/未来客户端只消费本协议(禁链接科学库绕过 CLI)。
- schema: `schemas/jsonl_event_v1.schema.json`(API-002 建立,CLI-002 golden 用)。

## 5 取消与崩溃(04 §5)

- Ctrl-C/Windows console cancel→协作取消令牌(acs_cancel, API-001 §2);内核在 ALG 5c 冻结的安全点检查。
- 取消后: 关 writer→写 incomplete manifest→删除/隔离临时产物→exit 9;**不得留下看似完整的 HiPS/结果**(与 ARCH-002 §5/ARCH-005 §3 原子单元一致)。
- 未捕获异常→70+run_id/阶段/最小脱敏 crash report(不泄露凭据)。

## 6 机器化一致性检查器合同(04 §6,API-002 建立 `tools/check_cli_protocol.py`)

1. `--help` golden 树与 §1 逐行一致;
2. JSON/JSONL 样例对 schema 有效(jsonschema 或 stdlib 等价校验);
3. 退出码常量唯一源(include/astrocs/exit_codes.h, grep 无第二处数值表);
4. handler→Phase API 追溯表存在且逐行有 API id;
5. 发布 manifest 不含旧 Phase exe(与 PRODUCTION_EXECUTION_INVENTORY production exe=0 联动, CLI-001 后=恰一 astrocs);
6. 双平台 golden command tests 字段+退出码一致(Windows 侧 Fatduck 执行)。

1–5 为 Linux 可验;6 属 WIN/FAT 域任务。

## 7 配置与 `output_dir`(FIX-E2E B1-A8 订正;权威 = `ASTROCS_DESIGN.md` §6.3 配置/退出码 + `ENGINEERING_SPEC.md` §7 目录规范)

运行产物(每相 run manifest `astrocs_run_*.json`、资源三件套
`resource_timeseries.csv` / `resource_summary.json` / `worker_balance.csv`、
`alloc_samples.csv` / `alloc_report.json`、节点科学产物)**只落 `output_dir`**;
CLI 不得以进程 CWD(`"."`)作为隐式缺省写出,否则在工作区根散落产物并触发
UT-CLI `mutates_workspace=false` 的 dirty 判定。

1. **必填**:`normalize|mosaic|export --json <config.json>` 的运行配置(CLI-001 唯一命令树;旧 `phase1|2|3` 的 `run`/`plan`/`validate`/`inspect` 用户命令已全部删除且 rc=2,见 §1 —— 「能力删除」登记:独立 `validate`/`plan`/`inspect` 命令面在新树下无载体,运行前预检由 §3.5 绿/橘/红页面 + `-y`/`-force` 承接,运行计划由产物 `run-plan.json`/`run-graph.json` 承接),
   无论 V1 顶层形态(`inputs`)还是平铺会话形态(`input_lights`/`hips_paths`/
   `phase3`),都必须显式给出 **非空字符串** `output_dir`。
2. **缺失/非串/空串**:平铺会话形态 → 配置错 `exit 2`(禁 silent default);
   V1 顶层形态 → `exit 3`(见 `lib/infrastructure/cli/parser.cpp` `validate_config_full`)。
3. **存在性**:V1 顶层形态要求 `output_dir` 目录已存在(`exit 3` if not found);
   平铺会话形态由 session/节点自建输出目录,不要求预先存在。
4. **取消路径**:SIGINT 后的 `incomplete` manifest 也写 `output_dir`,
   不再写 CWD `"."`。
5. 回归锚: `tests/cli/test_cli001_vpi.py`、`tests/cli/test_phase123_pipeline.py`
   负例矩阵、`run/release-rescue/fix-e2e/e2e_smoke.sh` 的
   `neg: missing output_dir` + `no CWD residue` 两条。
