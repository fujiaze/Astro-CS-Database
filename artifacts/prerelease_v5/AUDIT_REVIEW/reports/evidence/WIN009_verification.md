# WIN-009 验证报告 — Windows alpha amd64 发布包 + SBOM/licenses/hash/smoke + 无profile基线多线程fallback

结论: **PASS**。单 `astrocs.exe`+私有运行时 DLL; alpha 名; baseline 多线程 fallback; 解包 PASS。包在 Fatduck `artifacts/prerelease_v5/win_pkg/`。

## 1. 判据(03 L153 / 09 §5)
> 单 `astrocs.exe`+私有 DLL; alpha 名; baseline 多线程 fallback; 解包 PASS。
> 发布物 `AstroCS-Windows-amd64-<X.Y.Z-alpha.N>.zip` + MANIFEST/SHA256/SBOM/licenses。

## 2. 发布包(clean main `ef0858c`)
- 路径: `artifacts/prerelease_v5/win_pkg/AstroCS-Windows-amd64-0.9.0-alpha.1.zip`
- **zip sha256**: `ab269d93d5878f165a45b7b5b3bde56546711aaf8c2c76f1c2e72da8803ebf13`
- exe `--version`: `astrocs 0.9.0-alpha.1+gef0858c55a42`(干净, 无 `.dirty`; 工作树 DIRTYCOUNT=0)
- 工具: `tools/make_windows_release.py`(单一 exe + 私有 CRT/DLL + manifest + SBOM/licenses + hash)

### 包内容(解包 `astrocs/`)
`astrocs.exe` + `vcruntime140.dll` + `msvcp140.dll` + `vcruntime140_1.dll` + `vcomp140.dll`(私有运行时) + `VERSION` + `backends.manifest.json` + `SBOM.spdx.json` + `MANIFEST.json` + `SHA256SUMS` + `LICENSES/NOTICE.txt`。总计 10 文件(MANIFEST 登记 9 项, SHA256SUMS 于外层)。exe DLL 依赖: MSVCP140/VCOMP140/VCRUNTIME140/VCRUNTIME140_1 + 系统 api-ms-win-crt-*/KERNEL32(不随包)。

## 3. Smoke / 解包 PASS(Fatduck)
- `Expand-Archive` 解包 → `astrocs/` 9+1 文件就位。
- `astrocs.exe --version` → `0.9.0-alpha.1+gef0858c55a42`; EXIT=0。
- `astrocs.exe doctor --json` → `verdict: PASS`; baseline_selftest / hardware_sanity / backends_manifest(no shipped DSO, builtin baseline) 全 pass。
- **SHA256SUMS 逐文件校验全部 True**(MANIFEST/SBOM/VERSION/astrocs.exe/4 DLL/NOTICE)。

## 4. 无profile baseline 多线程 fallback
- 以**不带** `--cpu-profile` 跑 `phase1 run`(真银心 6 帧配置):
  `[phase1] session run: omp threads=16 (budget injected, cpus=16)`, EXIT=0, `frames=6`。
- 说明: 无 profile 时走保守 amd64 **baseline** backend + **多线程**(按 affinity 16 核注入预算), 非单线程; 符合 00 硬约束 #8。

## 5. 提交
- 源码/工具已于 `b8ce366`(make_windows_release.py); 本报告+ledger PASS 于 `WIN-009` commit。
- `02_TASK_LEDGER.csv` WIN-009 → PASS。

## 6. 限制/遗留
- 发布包为 Fatduck 本地产物; 按 09 §5 由最终审核通过后再创建源码 tag。
- Windows alpha 已产生; Linux alpha(LNX-005)`make_linux_release.py` 需在最终 SHA 上另产(见后续/REL-003, 不在 WIN-009 范畴)。
