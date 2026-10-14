# S09 证据：`normalize → mosaic → export` 真实帧端到端实跑

分支 `audit/acsd-science-docs-p5`。产品 `build/linux-sci/acsd`（Ninja/Release，基线 `e31b2be66`）。
输入与母版取自 `实验/shared/data_catalog.json`（按 AGENTS §13.1 由索引枚举，不按目录名猜测）。

## 1 输入（真实帧 + 真实母版，`testdata/` 只读、未复制、未上传）

| 角色 | 路径 |
|---|---|
| 光源 ×2 | `testdata/LDN43_T2素材_flying_dutchman/lights/LDN43_LRGBH_flying_dutchman-20250503@032713-1200S-Red.fts`、`…-20250504@032556-1200S-Red.fts` |
| master_bias | `testdata/T2 calibration files/masterBias_BIN-1_4096x4096.xisf`（sha256 `2d7e694c…`） |
| master_dark | `testdata/T2 calibration files/masterDark_BIN-1_4096x4096_EXPOSURE-1200.00s.xisf`（sha256 `73d42c70…`，曝光与光源一致 1200 s） |
| master_flat | `testdata/T2 calibration files/masterFlat_BIN-1_4096x4096_FILTER-Red_mono.xisf` |
| Gaia 库 | `gaia/GaiaDR3`（`gdr3-1.0.0-0{1..5}.xpsd`） |

## 2 结果：三命令**全部 rc=0**

| 命令 | `kind=final` | run manifest sha256 | 主要产物 |
|---|---|---|---|
| `acsd normalize --json normalize.json -y` | `status=ok`，`phase1 complete (blocks=1)`，`exit_code=0` | `e134d416f50dab8b8fa56264f9dca80b98a367928e144fe21c34ec0e5b9471bd` | 逐帧 HiPS：`<frame>/{ivar,variance}/Norder0..9 + Moc.fits + metadata.fits`；`master_refs.json` |
| `acsd mosaic --json mosaic.json -y` | `status=ok`，`phase2 complete`，`exit_code=0` | `2813433ada808401c0774632a60f00547c5ba8278814b1b53dc17ce9dc4ea4f5` | 马赛克 HiPS |
| `acsd export --json export.json -y` | `status=ok`，`phase3 complete`，`exit_code=0` | `48e87616c4f37e7393401ffc896a5f64fff6e86188ab55eb9bf3a2702d2db11a` | `output_phase3.fits`、`p3_verify.json`、`p3_resampled.{bin,json}`、`graph/` |

`export` 产物的独立复核（`astropy` 直接读回）：

```
shape (1024, 1024)  dtype >f4  finite_frac 1.0
CTYPE1/2 = RA---TAN / DEC--TAN
CRVAL1/2 = 248.609646405774 / -15.7591388976887   # 与输入帧 CRVAL 一致（索引值同）
CD1_1 = -0.001        BUNIT = ADU/sr
```

`p3_verify.json`：`canonical_match=true`、`coverage_ok=1`、
`coverage_stats={covered_px:1048576, total_px:1048576}`（**100% 覆盖**）、`reopen_ok=1`、
`integrity_sha256=3b2af8bd…`、`input_manifest_hash=351b1f01…`、`bunit=ADU/sr`、`coordinate_frame=equatorial`。
`p3_resampled.json`：`measurement_capable=true`、`order_sel=7`、
`pixel_semantics=surface_brightness`、`pixel_area_power=-2`、`bitpix=-32`。

## 3 三次 `fail-closed`：产品对**未声明的科学约定**拒绝静默默认（本轮最有价值的证据）

配置从 `--template` 派生后**故意**不声明科学约定，产品逐次以语义明确的错误码拒绝运行，
且**每次都没有写出半个产品树**（只落 run manifest 与资源台账）：

| 次序 | 错误码 | 原文要点 | 语义 |
|---|---|---|---|
| 1 | `MASTER_DARK_CONVENTION_UNDECLARED` | 「requires an explicit `dark_optimization` (true = master dark includes bias / false = bias-subtracted). The two conventions are different scientific inputs (SCI-CAL-001 §5) and must not default silently」 | 两种暗场约定是**不同科学输入**，不得静默取默认 |
| 2 | `MASTER_UNIT_MISMATCH` | 「master_bias … observed median=0.0152875 is in the normalized [0,1] domain while the light frame median=2308 ADU: master and light must share one scale」 | 母版与光源必须**同一量纲** |
| 3 | `MASTER_FLAT_NOT_NORMALIZED` | 「observed median=13525.2 outside `master_flat_median_range=[0.5,2]`: the master flat must be normalized to median=1.0 (SCI-CAL-001 §5/§6)」 | 平场必须归一化，或显式声明 `master_flat_normalize="median"` |

另有一条**非阻塞**警告（第 2 次运行起出现），已登记：
`acsd: cpu profile invalid: schema != acsd.cpu-profile/v2 — rerun 'acsd benchmark cpu'` →
`cpu_profile '/workspace/.local/share/ACSD/cpu_profile.json' rejected (rc=5) — conservative baseline`。
即本机缓存的 CPU profile **过期**，产品退回保守基线（正确行为，但该缓存应由 `acsd benchmark cpu` 重建）。

最终生效的配置增量（仅供复跑，**不是产品默认**）：
`dark_optimization=false`、`master_units={light:"ADU",bias:"normalized",dark:"normalized",flat:"normalized"}`、
`master_scale={light:1,bias:65535,dark:65535,flat:65535}`、`master_flat_normalize="median"`。

## 4 诚实边界（**必须与上面的 rc=0 同时引用**）

1. **本次只验证「链路与合同」，不验证测光定标正确性**。上述 `master_scale=65535` 是为通过量纲检查而
   声明的**冒烟测试用值**，而该母版实际已归一化到 [0,1] ⇒ 导出 FITS 的绝对量级**不可信**
   （实测 median `1.36e13 ADU/sr`、min `−6.5e14`、max `3.2e15`）。**不得**把这些数值当作
   科学产品读数；正确口径下应先用 `acsd benchmark`/正确 `master_scale` 重建。
2. 本跑**不经过** `eng/tests/e2e/` 的 9 条 `E2E.*` 判据（它们仍静态跳过，见 `S09_E2E_STATUS.md`），
   故**不得**引用为「`E2E.*` 判据已通过」。
3. 只跑了 **1 个 block × 2 帧**、单一滤镜（Red）、单一望远镜数据集（T2）；**未**跑多 block、
   **未**跑真实 `mosaic` 的多帧相对定标（此处只有 2 帧）、**未**跑 GUI 视觉验收。
4. 未做：失败注入的原子性验证（除上述三次天然 fail-closed 外）、FP32/Windows 端到端、
   `acsd benchmark` 全量、`doctor` 的 avx512 腿（本机 ISA 不支持，`skipped`）。

## 5 `acsd doctor --json`（环境）

```
verdict = PASS
checks: baseline_selftest pass | hardware_sanity pass | backend_provider_dir pass
        (build/linux-sci/providers) | backend_preflight:avx2 pass
        | backend_preflight:avx512 skipped ("unsupported ISA for acsd_cpu_avx512.so
          (required=928 detected=31)")
```

## 6 复现命令

```bash
cmake -S . -B build/linux-sci -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build/linux-sci -j 14
./build/linux-sci/acsd doctor --json
./build/linux-sci/acsd normalize --json /tmp/s09e2e/normalize.json -y
./build/linux-sci/acsd mosaic    --json /tmp/s09e2e/mosaic.json    -y
./build/linux-sci/acsd export    --json /tmp/s09e2e/export.json    -y
# 配置由 `acsd {normalize,mosaic,export} --template` 派生，增补见上文 §3 末
```
