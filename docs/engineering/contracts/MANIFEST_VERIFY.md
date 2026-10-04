# 运行清单与校验合同

上游：最高设计的 I/O 与原子产品一章。

运行清单的字段、分离原则、溯源子对象与校验顺序的正本。科学配置与 CPU 机器画像是两个独立文件、
独立校验、独立散列，画像陈旧即确定错误，不做猜测。退出码语义见 `LOG_AND_ERROR.md`。

## pipeline_config.json v1（配置校验 schema）

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
cpu profile(独立文件): **契约唯一源 = `eng/contracts/schemas/cpu_profile.schema.json`**（CFG-001 单文件双分支：legacy_v1 `schema_version=1` + profile_v2 `schema=acsd.cpu-profile/v2`；`x-acsd-writer` 声明生产者仅 `benchmark`）；校验 oracle = `eng/tools/validate_cpu_profile.py`（schema 最小校验 + stale 判定，消费面 `eng/tests/backend/test_cpu_profile.py`）；无/失配 profile → 回落 generic(baseline) + 动态多线程，不阻塞（`profile_store.h`）。CLI 侧运行时校验入口 `lib/infrastructure/cli/parser.cpp`（`validate_cpu_profile`）当前无生产调用方——消费链接线缺口已在死键台账（`eng/contracts/ledgers/dead_config_keys.json`）与开放项清单登记在案，处置排期随该登记推进。

## run_manifest.json v1(run 结束原子写, ARCH-002 「落点映射与测试」一节)

```json
{ "schema_version":"1", "kind":"acsd_run_manifest", "run_id":"<12hex>",
 "acsd_version":"<X.Y.Z-alpha.N+g12hex>", "platform":{"os":"linux|windows","arch":"amd64"},
 "config_path": "<utf-8>", "cpu_profile_path": "<utf-8|null>",
 "config_sha256":"<hex>", "cpu_profile_sha256":"<hex|null>", "phases":[1,2,3],
 "artifacts":[{"role":"phase3_output","path":"<rel>","sha256":"<hex>","size_bytes":N}],
 "status":"complete|incomplete", "started_utc":"...", "finished_utc":"..." }
```

- `config_sha256`/`cpu_profile_sha256` 记录**输入文件字节 hash**(verify 重算比对;路径由 `config_path`/`cpu_profile_path` 提供)。
- 取消/崩溃/not-wired stub → `status:"incomplete"` manifest(**complete manifest 只出自完成的科学运行**——run 命令 stub 亦写 incomplete 并 exit 2);atomic tmp+rename。

### provenance 子对象（加性扩展；最高设计的 manifest 必记项）

`run` 命令在 run manifest 顶层追加 `provenance` 对象（additive；v1 校验器/
`verify` 忽略未知顶层键，向后兼容）。字段全部**由真实节点 manifest 汇总**，
不写占位串；缺省即空数组而非伪造值：

```json
"provenance": {
 "source_sha": "<40hex>", // CMake **configure 期** HEAD（version_generated.h）——
 // 不是构建指纹；相等不蕴含同一二进制（「provenance 子对象」一节）
 "source_version": "<X.Y.Z-alpha.N+g<12hex>>",
 "build_source_digest": "<64hex>", // **构建期**源集内容摘要 = 构建指纹（RUN-PROVENANCE-01）
 "build_head_sha": "<40hex>", // 构建期 HEAD（人读补充，不得单独当指纹）
 "build_dirty": false, // 构建期工作树是否有未提交改动
 "configure_head_sha": "<40hex>", // configure 期 HEAD（对照 source_sha 的来历）
 "algorithm_ids": ["<ALG-id>"], // 各节点 manifest.algorithm_id 去重
 "module_build_ids": ["<module_id>@<version>"],
 "providers": ["baseline"],
 "units": ["ADU"], // 节点 manifest.bunit 去重
 "coordinate_frames": ["equatorial","icrs"],
 "input_product_hashes": [{"node":"<node_id>","sha256":"<64hex>"}],
 "output_product_hashes":[{"role":"phase3_output","path":"<rel>","sha256":"<hex>"}]
}
```

- Phase3 writer 节点另落 `run_context.json`（`<out_dir>/run_context.json`，原子写：
 `schema_version/kind/run_id/software_version/source_sha` + 构建指纹
 `build_source_digest/build_head_sha/build_dirty/configure_head_sha`，语义见
 「provenance 子对象」一节；构建指纹缺失时写入 fail-closed，不产出不可锚的上下文）；
 `p3_op_writer` 在缺失或
 `run_id`/`software_version` 为空时 fail-closed，把输入 HiPS `signal/properties` +
 `signal/Moc.fits` 的 sha256 作为 `input_manifest_hash` 注入 FITS HISTORY 与 provenance。

### 加性顶层键 `storage`（运行级形态事实；R-42/P-181）

`run` 命令在 run manifest 顶层**可选**追加 `storage` 对象（additive，与 「provenance 子对象」一节 的
`provenance` 同形：v1 校验器/`verify` 忽略未知顶层键，向后兼容）。**条款归属**：该键的
字段词表与不变式 M1..M4 的**唯一正本** = `HIPS_STORAGE_FORM.md` 「运行完成清单 storage 段」一节
（本文件只登记键的存在与归属，不复写字段）；**唯一机器事实源** =
`eng/contracts/schemas/hips_storage_form.schema.json` 的 `$defs.manifest_storage`；CFG-001
`eng/contracts/schemas/run_manifest.schema.json` 只登记该键位与类型。
缺失 ⇒ 无形态事实（**不判红**）；出现 ⇒ 必须逐条满足 M1..M4（运行清单 schema 校验项的加性键
白名单须已登记 `storage`）。**产品级**完成 manifest
（`products/{user_path}/manifest.json`）
**不承载**该键——其形态事实只在 `tree`/`tree_hash`/`fitsverify` 三项
（「落点映射与测试」一节）。

## manifest verify 合同(acsd doctor --json --run-manifest <manifest.json>)

独立 `verify` 命令不在命令面上（CLI-001 唯一命令树；verify* 为已删别名 → rc=2，负例锁定于
`eng/tests/cli/test_cli_protocol.py` test_03）。manifest verify 的现行载体 = **`doctor` 的机器旗标
`--run-manifest`**（`lib/infrastructure/cli/commands.cpp` → `cmd_verify`）。
校验序→错误码: manifest 语法/schema(3)→status=="complete"(否则 8)→acsd_version 与本机一致(5, 版本不同不可 verify)→重算 config/profile hash(3, 输入已变)→逐 artifact 存在性(3)+sha256(8)+size(8)→全部过→0 并输出 JSON `{verify:"ok", checked:N, manifest:<path>}`（stdout 恰一个 JSON 文档）。

## config/profile 分离校验落点

config 与 cpu profile 的分离校验由 `normalize|mosaic|export` 运行前预检
（`lib/infrastructure/cli/subcommand.h` `precheck_config`）与 `doctor --json` 承接
（现行校验入口声明：`lib/infrastructure/cli/commands.cpp`）；独立 `show-effective` 命令
无载体（CLI-001 唯一命令树，调用 rc=2）。

## 落点映射与测试

- manifest 原子写: `lib/infrastructure/cli/commands.cpp` `write_run_manifest`
 （tmp+rename；stub/not-wired/cancelled 恒 `incomplete`）；provenance 子对象 =
 `commands.cpp`（含构建期指纹锚）；artifact 规范化哈希 = `commands.cpp` 两处。
- config 校验: `lib/infrastructure/cli/parser.cpp` `validate_config_full`
 （V1 顶层形态 → exit 3，与运行前预检同源）。
- manifest verify: `acsd doctor --json --run-manifest`（「manifest verify 合同」一节，`cmd_verify`）。
- hash 工具: CRYPTO 公共层 `lib/algorithms/shared/crypto` sha256（CLI 侧封装 `file_sha256`，
 `commands.cpp` 三处在役调用）。
- golden: `eng/tests/cli/test_cli_protocol.py`（schema_version 篡改→2 / 未知键 / 路径不存在 /
 manifest verify 全组 test_01..test_06 / 独立 verify 命令保持删除 test_07），每组断言退出码。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md，最高设计`，上位来源。
[2] 内部文档 `docs/engineering/contracts/LOG_AND_ERROR.md`，同层相关正本。
[3] 内部文档 `docs/engineering/build/RELEASE.md`，同层相关正本。
