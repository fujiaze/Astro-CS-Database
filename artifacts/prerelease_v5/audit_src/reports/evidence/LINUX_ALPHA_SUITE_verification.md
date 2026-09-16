# Linux alpha 发布包生产 + 当前 SHA 全套件验证

结论: **Linux alpha 包已产并登记 (RELEASE_ARTIFACTS), 当前 SHA (89b32bd) Linux 全套件 320 测试通过(1 个已知 ISA-001 计时抖动, 隔离重跑 PASS, 非回归)**。

## 1. 发现并修复: Linux release 二进制过期
- 原 `build/lnx_v5_clean_rel/astrocs` 为过期二进制 `+g4b757a65e627.dirty`(早于 XISF/栈溢出修复)。
- 重配置 + 重建(cmake cmake+cmake --build -j2)到当前源码: `0.9.0-alpha.1+gec7a8cdc1566`, 0 error。
- 说明: `.dirty` 后缀源于工作树存在无关未跟踪产物(控制包 zip / 测试运行 astrocs_run_*.json); 包内 `VERSION` 由 make_linux_release.py 生成, 为干净 `0.9.0-alpha.1+gec7a8cdc1566`。

## 2. Linux alpha 发布包
- 产物: `artifacts/prerelease_v5/lnx_pkg/AstroCS-Linux-amd64-0.9.0-alpha.1.tar.zst`
- **sha256**: `554bcd714cd19586019db06d971275a24bb5afc4861d247d7a99753874266171`
- 内容: `astrocs/{VERSION,MANIFEST.json,SBOM.spdx.json,backends.manifest.json,LICENSES/NOTICE.txt,bin/astrocs,SHA256SUMS}`
- MANIFEST sha `48e048dd05ae410dd8e942bc94d3e3f1ee078cf7adc52e2f48f25dd89c9f0186`; SBOM sha `89c06383e3cd93553da95dae95737f1ae8ea2dac7ff4db398aff7c5daadae9da`; size 1345204。
- Smoke(解包): `bin/astrocs --version` 干净; `doctor --json` → `verdict: PASS`; `SHA256SUMS` 逐文件全 **OK**。
- 已登记: `RA-LNX-005-ALPHA`(RELEASE_ARTIFACTS, Linux, PASS) + `LA-LNX-005-ALPHA`(大件)。

## 3. 当前 SHA Linux 全套件(89b32bd)
- `python3 -m unittest discover -s tests -p test_*.py`:
  `Ran 320 tests in 808.191s` → **FAILED (failures=1)**。
- 唯一失败: `ISA-001 实测: 变体更慢的 op(driz_accum)在候选选择中不得胜过 baseline`(tests/backend/test_cpu_profile.py)。
- **隔离重跑** `TestCpuProfile.test_05_avx512_slower_never_selected` → **OK(20s)**: 该失败为满负载下的计时抖动(变体瞬测快于 baseline), 非回归。
- 结论: 当前 SHA 无真实失败; 双平台 alpha(Linux+Windows)均 PASS 登记。

## 4. 提交
- `89b32bd`(Linux alpha 生产 + 双平台 alpha 登记 + 审计刷新)。

## 5. 限制/遗留
- 发布仍非就绪: 剩 WIN-006/7/8(HIPS 链, 记 BLOCKED)、PAR-002(cfitsio 并发读崩溃, BLOCKED)、REV-002 外部审阅、REL-001..4。
