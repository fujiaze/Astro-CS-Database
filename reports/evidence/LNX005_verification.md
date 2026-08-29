# LNX-005 验证报告 — Linux amd64 alpha 发布包(SBOM/manifest/hash/smoke)

结论: **PASS**。

## 1. 验收判据(03_TASK_DETAILS.md L143)
> staging install、package whitelist、manifest/SBOM/licenses/hash; 在空临时目录 smoke。
> PASS = 只有一个 user exe; 私有 SO/manifest 完整; 包名 alpha; 解包运行 PASS。

## 2. 交付物
- `tools/make_linux_release.py`: Linux amd64 alpha 发布包生成器(09 §5 命名 + 13 § 版本单源 + 05 §7 manifest)。
  生成的包:
  ```
  AstroCS-Linux-amd64-0.9.0-alpha.1.tar.zst   (+ .sha256)
  └─ astrocs/
     ├─ bin/astrocs                (唯一用户可执行)
     ├─ VERSION                     (0.9.0-alpha.1+g<commit12>, 读 VERSION 源)
     ├─ backends.manifest.json      (05 §7; builtin baseline 无 shipped DSO → 空 backend 表)
     ├─ MANIFEST.json               (每文件 path/sha256/size/mode)
     ├─ SBOM.spdx.json              (SPDX 2.3, 包名 alpha)
     ├─ LICENSES/NOTICE.txt
     └─ SHA256SUMS                  (SHA256SUM -c 可校验)
  ```
- `tests/quality/test_linux_release.py`: 6 用例。

## 3. 测试结果
`python3 -m unittest tests.quality.test_linux_release` → **OK(6 用例)**。
| 用例 | 验收点 | 结果 |
|---|---|---|
| test_01_package_created | tar 包生成, 名含 AstroCS-Linux-amd64- | OK |
| test_02_package_name_is_alpha | 包名版本 = VERSION 源(0.9.0-alpha.N), 禁 stable/rc/release/1.0 | OK |
| test_03_single_user_exe_and_tree | bin/ 恰一个 user exe = astrocs; 无旧 phase/benchmark/tool exe 泄漏 | OK |
| test_04_manifest_sbom_licenses_hash_present | MANIFEST/SBOM.spdx/VERSION/SHA256SUMS/backends.manifest/LICENSES 齐 | OK |
| test_05_manifest_entries_match_files | 每 manifest 文件真实存在+sha256 一致; SHA256SUMS -c 全 OK | OK |
| test_06_extracted_run_doctor_passes | 空临时目录解包 → `astrocs doctor --json` verdict=PASS; `--version` 可运行 | OK |

配套手工核验(空目录解包):外层 `.sha256` 与 tar 一致(HASH_OK);包内可执行文件 `find -type f -executable` 仅
`astrocs/bin/astrocs`;解包 `astrocs doctor --json` → PASS。

## 4. 关键核验(对应 L143 PASS 判据)
- **只有一个 user exe**: 包树 `bin/` 仅 `astrocs`;`find -type f -executable` 无其它可执行;CLI-008
  `test_cli_single_install`(staging install, 5 用例)亦 PASS。
- **私有 SO/manifest 完整**: `backends.manifest.json`(05 §7)+ `MANIFEST.json` + `SBOM.spdx.json` +
  `LICENSES/NOTICE.txt` 齐备;本包为 builtin baseline(无 shipped DSO), 故 backend 表为空并注明。
- **包名 alpha**: 版本单源 `VERSION=0.9.0-alpha.1`;包名 `AstroCS-Linux-amd64-0.9.0-alpha.1.tar.zst`;
  13 § 明确本包只产 alpha,禁 stable/rc/1.0。
- **解包运行 PASS**: 空临时目录解包 `tar --zstd -xf` → `astrocs doctor` verdict=PASS。

## 5. 限制
- 二进制由当前工作区构建,其内嵌 build_id 带 `.dirty`(工作区含大量 pre-existing 未提交文件, 属既定
  环境, 不得提交/丢弃);发布包内 `VERSION`/`MANIFEST.json` 记录的是干净 `0.9.0-alpha.1+g<commit12>`
  (无 dirty)。若最终要求二进制本身也 clean,需在干净 clone 上重建(本阶段不可行, 已如实记录)。
- SBOM 为包级(SPDX 2.3)建档, 因本包为单一 CLI 且静态自带库, 不逐第三方库枚举二进制清单(许可证
  来源见仓库 third_party/LICENSE 与 PACKAGE_MANIFEST)。
- 打包用 `tar --zstd`(09 §5);若 zstd 不可用则 `--tar-gz` 回退并记录。
