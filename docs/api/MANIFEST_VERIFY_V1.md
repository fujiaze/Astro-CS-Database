# AstroCS run manifest 与 verify 合同 v1 (CLI-003 冻结)

> ID: API-MANIFEST-001  状态: FROZEN (V5 CLI-003, 2026-08-28)  上游: API-002(final 事件/verify 命令)/04  §4-5  下游: CODE-P*/TST-P*/REL-002
> 分离原则(硬性): **科学 config(pipeline_config.json)与 CPU profile(cpu_profile.json)是两个独立文件、独立校验、独立 hash**;profile 陈旧→确定错误, 不猜测。

## 1 pipeline_config.json v1(config init/validate/show-effective 的 schema)

```json
{
  "schema_version": "1",
  "inputs": { "lights": ["<path>"], "darks": [], "flats": [], "bias": [] },
  "output_dir": ".",
  "phase3": { "source": {"hips_dir": "<path>"}, "center": {"ra_deg": 0.0, "dec_deg": 0.0},
              "scale_deg_per_px": 0.0, "width_px": 0, "height_px": 0 }
}
```

validate 校验序(错误码确定, 不猜测): JSON 语法(3)→顶层对象+schema_version=="1"(3)→inputs 四键存在且为字符串数组、路径非空且文件存在(3)→output_dir 存在(3)。已知键白名单外键→3(防拼写静默忽略)。schema_version≠"1"→2(参数/配置错, 与输入缺失区分)。
cpu profile(独立文件, `{"schema_version":"1","kind":"astrocs_cpu_profile","cpu_signature":"<hw hash>","kernels":{...}}`): validate 仅做 kind/schema_version/结构检查(3);**stale 判定=profile.cpu_signature ≠ 本机 signature → verify/show-effective 报 5(backend/CPU 特征)**, 属 04 表。

## 2 run_manifest.json v1(run 结束原子写, ARCH-002 §5)

```json
{ "schema_version":"1", "kind":"astrocs_run_manifest", "run_id":"<12hex>",
  "astrocs_version":"<X.Y.Z-alpha.N+g12hex>", "platform":{"os":"linux|windows","arch":"amd64"},
  "config_path": "<utf-8>", "cpu_profile_path": "<utf-8|null>",
  "config_sha256":"<hex>", "cpu_profile_sha256":"<hex|null>", "phases":[1,2,3],
  "artifacts":[{"role":"phase3_output","path":"<rel>","sha256":"<hex>","size_bytes":N}],
  "status":"complete|incomplete", "started_utc":"...", "finished_utc":"..." }
```

- `config_sha256`/`cpu_profile_sha256` 记录**输入文件字节 hash**(verify 重算比对;路径由 `config_path`/`cpu_profile_path` 提供)。
- 取消/崩溃/not-wired stub → `status:"incomplete"` manifest(04 §5;**禁止无科学运行的 complete manifest**——run 命令 stub 亦写 incomplete 并 exit 2);atomic tmp+rename。

## 3 verify 合同(astrocs verify --run-manifest --json)

校验序→错误码: manifest 语法/schema(3)→status=="complete"(否则 8)→astrocs_version 与本机一致(5, 换版本不可 verify 旧 run)→重算 config/profile hash(3, 输入已变)→逐 artifact 存在性(3)+sha256(8)+size(8)→全部过→0 并输出 JSON `{verify:"ok", checked:N, manifest:<path>}`。

## 4 show-effective 合同

`--config --cpu-profile --json`: 两者分别 validate(错误码同 §1)→profile stale(5)→输出 `{schema_version:"1", config:<原文>, cpu_profile:<原文>, effective:{phases:[...], profile_signature:...}}`。人类模式无 --json 时拒绝(2, 04 §1 该命令固定 --json)。

## 5 任务映射与测试

- 实现: cli/main.cpp(cmd_config_validate 强化/cmd_show_effective/cmd_verify/cmd_run 收尾写 manifest);golden: tests/cli/test_cli_protocol.py 追加 mutation 组(schema_version 篡改/未知键/路径不存在/hash 篡改/stale profile/status incomplete/版本不一致), 每组断言退出码。
- hash 工具: cli 内 sha256 实现(CRYPTO 公共层 lib/common/crypto 已有 sha256, 链接复用; CLI 侧封装 file_sha256)。
