# AstroCS CLI 协议合同 v1 (API-002 冻结 — 权威=控制包 04)

> ID: API-CLI-001  状态: FROZEN (V5 API-002, 2026-08-28)  上游: API-001/ARCH-002  下游: CLI-001/002/003, API-003..005(handler 追溯), BENCH-005
> 本文件为控制包 04 的仓库落地;两者冲突以 04 为准并在 traceability 登记。

## 1 命令树(04 §1 逐条,help 文本 golden 由此生成)

```text
astrocs --version [--json]
astrocs hardware inspect --json
astrocs config init --output <path>
astrocs config validate --config <path>
astrocs config show-effective --config <path> [--cpu-profile <path>] --json
astrocs benchmark cpu (--quick|--full) [--output <path>] [--events-jsonl]     # 唯一用户 benchmark 入口,禁另发 benchmark exe
astrocs doctor --json
astrocs test synthetic --group <all|calibration|wcs_psf|noise_snr|drizzle|upm|rejection_integration|pipeline>
astrocs phase1 run --config <path> [--cpu-profile <path>] [--events-jsonl]
astrocs phase2 run --config <path> [--cpu-profile <path>] [--events-jsonl]
astrocs phase3 run --config <path> [--cpu-profile <path>] [--events-jsonl]
astrocs run --phases <1|2|3|1,2|1,2,3> --config <path> [--cpu-profile <path>] [--events-jsonl]
astrocs verify --run-manifest <path> --json
```

handler→Phase API 追溯(04 §6-4): phase1 run→API-003 create/validate/run/inspect;phase2 run→API-004;phase3 run→API-005;test synthetic→SYN-00x Oracle 入口;benchmark cpu→BENCH-001..004 harness(内部);verify→manifest 复算。

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
