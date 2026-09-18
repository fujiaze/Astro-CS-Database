# module_adapters.cpp 探针插入清单（RELEASE-02 / PROBE）

> ⚠️ 本文件不修改 `lib/infrastructure/scheduler/src/module_adapters.cpp`（HUB-C 正在改该文件）。
> 以下为**精确插入清单**，由前台在 HUB-C 落地后统一应用。
> 机器可应用版本：`run/RELEASE-02/probe/module_adapters_probe_insertions.json` +
> `python3 run/RELEASE-02/probe/apply_insertions.py [--apply]`（按精确锚点文本定位，抗行号漂移；
> 锚点非唯一即报错不写；已应用过则跳过）。

- 目标文件：`lib/infrastructure/scheduler/src/module_adapters.cpp`（HEAD 行号；应用前请以锚点文本为准）
- 前置：CMake 已加 `ASTROCS_PROBES`（默认 OFF）+ `astrocs_probes` target，且 `astrocs_module_adapters` 已 link 之。
- 所有插入都是探针调用，**不改变任何科学数值/控制流**；OFF 时宏为空语句。

| # | id | 锚点行号 | 说明 |
|---|----|---------|------|
| 1 | `P1-include` | 79 | 探针头面 (ASTROCS_PROBES=OFF 时 astrocs/probe.h 全部宏为空语句) |
| 2 | `P1-stage-switch` | 6126-6135 | Phase1 七阶段边界: 每 case 一个 RAII 作用域 (OFF 时为空语句) |
| 3 | `P2-stage-switch` | 6317-6325 | Phase2 七阶段边界: coverage/sample/upm_fit/upm_apply/reject/integrate/write |
| 4 | `P1-calibrate-frame` | 1699-1700 | Phase1 逐帧热点: calibrate |
| 5 | `P1-wcs-frame` | 2763-2769 | Phase1 逐帧热点: wcs |
| 6 | `P1-drizzle-frame` | 3479-3483 | Phase1 逐帧热点: drizzle |
| 7 | `P2-reject-tile` | 4977-4978 | Phase2 逐 tile 热点: reject |
| 8 | `P2-reject-union-gauge` | 4953-4956 | 规模 gauge: 并集 tile 数 |
| 9 | `P2-integrate-tile` | 5477-5479 | Phase2 逐 tile 热点: integrate |
| 10 | `P2-integrate-tiles-gauge` | 5474-5477 | 规模 gauge: 待积分 tile 数 |
| 11 | `P2-skyplane-apply-tile` (可选) | 4676-4678 | 可选: upm_apply 逐 tile 的 sky_plane 应用上下文 (库层 eval_block 已计时) |

## 1. `P1-include`

- 位置：`lib/infrastructure/scheduler/src/module_adapters.cpp:79`
- 说明：探针头面 (ASTROCS_PROBES=OFF 时 astrocs/probe.h 全部宏为空语句)

锚点原文（前后文即锚点首/末行）：
```cpp
#include "crypto/sha256.h"         // astrocs::crypto::sha256_hex (input_manifest_hash)
```

替换为：
```cpp
#include "crypto/sha256.h"         // astrocs::crypto::sha256_hex (input_manifest_hash)
#include "astrocs/probe.h"         // RELEASE-02 探针 (ASTROCS_PROBES=OFF 时宏为空语句)
```

## 2. `P1-stage-switch`

- 位置：`lib/infrastructure/scheduler/src/module_adapters.cpp:6126-6135`
- 说明：Phase1 七阶段边界: 每 case 一个 RAII 作用域 (OFF 时为空语句)

锚点原文（前后文即锚点首/末行）：
```cpp
      switch (spec_.op) {
        case P1NodeOp::Calibrate:  r = p1_op_calibrate(doc, &man); break;
        case P1NodeOp::Cosmetic:   r = p1_op_cosmetic(doc, &man); break;
        case P1NodeOp::StarPsf:    r = p1_op_star_psf(doc, &man); break;
        case P1NodeOp::WcsSolve:   r = p1_op_wcs(doc, &man); break;
        case P1NodeOp::Photometry: r = p1_op_photometry(doc, &man); break;
        case P1NodeOp::NoiseSnr:   r = p1_op_noise(doc, &man); break;
        case P1NodeOp::Drizzle:    r = p1_op_drizzle(doc, &man); break;
        case P1NodeOp::Writer:     r = p1_op_writer(doc, &man); break;
      }
```

替换为：
```cpp
      switch (spec_.op) {
        // [RELEASE-02 probe] Phase1 七阶段边界 (calibrate/cosmetic/star_psf/wcs/noise/drizzle/writer)
        case P1NodeOp::Calibrate: {
          ASTROCS_PROBE_SCOPE("phase1", "calibrate");
          r = p1_op_calibrate(doc, &man); break;
        }
        case P1NodeOp::Cosmetic: {
          ASTROCS_PROBE_SCOPE("phase1", "cosmetic");
          r = p1_op_cosmetic(doc, &man); break;
        }
        case P1NodeOp::StarPsf: {
          ASTROCS_PROBE_SCOPE("phase1", "star_psf");
          r = p1_op_star_psf(doc, &man); break;
        }
        case P1NodeOp::WcsSolve: {
          ASTROCS_PROBE_SCOPE("phase1", "wcs");
          r = p1_op_wcs(doc, &man); break;
        }
        case P1NodeOp::Photometry: {
          ASTROCS_PROBE_SCOPE("phase1", "photometry");
          r = p1_op_photometry(doc, &man); break;
        }
        case P1NodeOp::NoiseSnr: {
          ASTROCS_PROBE_SCOPE("phase1", "noise");
          r = p1_op_noise(doc, &man); break;
        }
        case P1NodeOp::Drizzle: {
          ASTROCS_PROBE_SCOPE("phase1", "drizzle");
          r = p1_op_drizzle(doc, &man); break;
        }
        case P1NodeOp::Writer: {
          ASTROCS_PROBE_SCOPE("phase1", "writer");
          r = p1_op_writer(doc, &man); break;
        }
      }
```

## 3. `P2-stage-switch`

- 位置：`lib/infrastructure/scheduler/src/module_adapters.cpp:6317-6325`
- 说明：Phase2 七阶段边界: coverage/sample/upm_fit/upm_apply/reject/integrate/write

锚点原文（前后文即锚点首/末行）：
```cpp
      switch (spec_.op) {
        case P2NodeOp::Coverage:  r = p2_op_coverage(cfg2, &man); break;
        case P2NodeOp::Sample:    r = p2_op_sample(cfg2, &man); break;
        case P2NodeOp::UpmFit:    r = p2_op_upm_fit(cfg2, &man); break;
        case P2NodeOp::UpmApply:  r = p2_op_upm_apply(cfg2, &man); break;
        case P2NodeOp::Reject:    r = p2_op_reject(cfg2, &man); break;
        case P2NodeOp::Integrate: r = p2_op_integrate(cfg2, &man); break;
        case P2NodeOp::Write:     r = p2_op_write(cfg2, &man); break;
      }
```

替换为：
```cpp
      switch (spec_.op) {
        // [RELEASE-02 probe] Phase2 七阶段边界 (coverage/sample/upm_fit/upm_apply/reject/integrate/write)
        case P2NodeOp::Coverage: {
          ASTROCS_PROBE_SCOPE("phase2", "coverage");
          r = p2_op_coverage(cfg2, &man); break;
        }
        case P2NodeOp::Sample: {
          ASTROCS_PROBE_SCOPE("phase2", "sample");
          r = p2_op_sample(cfg2, &man); break;
        }
        case P2NodeOp::UpmFit: {
          ASTROCS_PROBE_SCOPE("phase2", "upm_fit");
          r = p2_op_upm_fit(cfg2, &man); break;
        }
        case P2NodeOp::UpmApply: {
          ASTROCS_PROBE_SCOPE("phase2", "upm_apply");
          r = p2_op_upm_apply(cfg2, &man); break;
        }
        case P2NodeOp::Reject: {
          ASTROCS_PROBE_SCOPE("phase2", "reject");
          r = p2_op_reject(cfg2, &man); break;
        }
        case P2NodeOp::Integrate: {
          ASTROCS_PROBE_SCOPE("phase2", "integrate");
          r = p2_op_integrate(cfg2, &man); break;
        }
        case P2NodeOp::Write: {
          ASTROCS_PROBE_SCOPE("phase2", "write");
          r = p2_op_write(cfg2, &man); break;
        }
      }
```

## 4. `P1-calibrate-frame`

- 位置：`lib/infrastructure/scheduler/src/module_adapters.cpp:1699-1700`
- 说明：Phase1 逐帧热点: calibrate

锚点原文（前后文即锚点首/末行）：
```cpp
  for (size_t fi = 0; fi < lights.size(); ++fi) {
    if (bias.ok()) {
```

替换为：
```cpp
  for (size_t fi = 0; fi < lights.size(); ++fi) {
    // [RELEASE-02 probe] 逐帧热点: calibrate
    ASTROCS_PROBE_SCOPE_CTX(_probe_cal_frame, "phase1", "calibrate.frame");
    ASTROCS_PROBE_TAG(_probe_cal_frame, "frame_key", p1_frame_key(lights[fi]).c_str());
    if (bias.ok()) {
```

## 5. `P1-wcs-frame`

- 位置：`lib/infrastructure/scheduler/src/module_adapters.cpp:2763-2769`
- 说明：Phase1 逐帧热点: wcs

锚点原文（前后文即锚点首/末行）：
```cpp
  for (const auto& l : doc["input_lights"]) {
    const std::string lp = l.get<std::string>();
    const std::string frame_path = p1_calibrated_path(doc, lp);
    P1Image im = p1_read_image(frame_path);
    if (!im.ok()) {
      (*man)["error_kind"] = "input";
      cleanup();
```

替换为：
```cpp
  for (const auto& l : doc["input_lights"]) {
    const std::string lp = l.get<std::string>();
    // [RELEASE-02 probe] 逐帧热点: wcs
    ASTROCS_PROBE_SCOPE_CTX(_probe_wcs_frame, "phase1", "wcs.frame");
    ASTROCS_PROBE_TAG(_probe_wcs_frame, "frame_key", p1_frame_key(lp).c_str());
    const std::string frame_path = p1_calibrated_path(doc, lp);
    P1Image im = p1_read_image(frame_path);
    if (!im.ok()) {
      (*man)["error_kind"] = "input";
      cleanup();
```

## 6. `P1-drizzle-frame`

- 位置：`lib/infrastructure/scheduler/src/module_adapters.cpp:3479-3483`
- 说明：Phase1 逐帧热点: drizzle

锚点原文（前后文即锚点首/末行）：
```cpp
  Json frame_entries = Json::array();
  bool have_first = false;
  for (const auto& l : doc["input_lights"]) {
    const std::string lp = l.get<std::string>();
    const std::string frame_path = p1_calibrated_path(doc, lp);
```

替换为：
```cpp
  Json frame_entries = Json::array();
  bool have_first = false;
  for (const auto& l : doc["input_lights"]) {
    const std::string lp = l.get<std::string>();
    // [RELEASE-02 probe] 逐帧热点: drizzle
    ASTROCS_PROBE_SCOPE_CTX(_probe_drz_frame, "phase1", "drizzle.frame");
    ASTROCS_PROBE_TAG(_probe_drz_frame, "frame_key", p1_frame_key(lp).c_str());
    const std::string frame_path = p1_calibrated_path(doc, lp);
```

## 7. `P2-reject-tile`

- 位置：`lib/infrastructure/scheduler/src/module_adapters.cpp:4977-4978`
- 说明：Phase2 逐 tile 热点: reject

锚点原文（前后文即锚点首/末行）：
```cpp
  for (const auto& [tip, refs] : union_tiles) {
    const uint64_t depth = refs.size();
```

替换为：
```cpp
  for (const auto& [tip, refs] : union_tiles) {
    // [RELEASE-02 probe] 逐 tile 热点: reject
    ASTROCS_PROBE_SCOPE_CTX(_probe_rej_tile, "phase2", "reject.tile");
    ASTROCS_PROBE_TAG(_probe_rej_tile, "tile_id", static_cast<unsigned long long>(tip));
    ASTROCS_PROBE_GAUGE("phase2", "reject.tile_frames", static_cast<double>(refs.size()));
    const uint64_t depth = refs.size();
```

## 8. `P2-reject-union-gauge`

- 位置：`lib/infrastructure/scheduler/src/module_adapters.cpp:4953-4956`
- 说明：规模 gauge: 并集 tile 数

锚点原文（前后文即锚点首/末行）：
```cpp
      union_tiles[tip].push_back(TileRef{tip, f, off});
    }
  }
  std::vector<uint8_t> accepted_bin;
```

替换为：
```cpp
      union_tiles[tip].push_back(TileRef{tip, f, off});
    }
  }
  // [RELEASE-02 probe] 规模 gauge: 并集 tile 数
  ASTROCS_PROBE_GAUGE("phase2", "reject.union_tiles", static_cast<double>(union_tiles.size()));
  std::vector<uint8_t> accepted_bin;
```

## 9. `P2-integrate-tile`

- 位置：`lib/infrastructure/scheduler/src/module_adapters.cpp:5477-5479`
- 说明：Phase2 逐 tile 热点: integrate

锚点原文（前后文即锚点首/末行）：
```cpp
  for (const auto& rt : rej_tiles) {
    const uint64_t tip = rt.value("tile_ipix", 0ull);
    const uint64_t rej_off = rt.value("offset", 0ull);
```

替换为：
```cpp
  for (const auto& rt : rej_tiles) {
    const uint64_t tip = rt.value("tile_ipix", 0ull);
    // [RELEASE-02 probe] 逐 tile 热点: integrate
    ASTROCS_PROBE_SCOPE_CTX(_probe_int_tile, "phase2", "integrate.tile");
    ASTROCS_PROBE_TAG(_probe_int_tile, "tile_id", static_cast<unsigned long long>(tip));
    const uint64_t rej_off = rt.value("offset", 0ull);
```

## 10. `P2-integrate-tiles-gauge`

- 位置：`lib/infrastructure/scheduler/src/module_adapters.cpp:5474-5477`
- 说明：规模 gauge: 待积分 tile 数

锚点原文（前后文即锚点首/末行）：
```cpp
  if (!rej_tiles.is_array() || rej_tiles.empty())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "rejection artifact tiles invalid"));
  for (const auto& rt : rej_tiles) {
```

替换为：
```cpp
  if (!rej_tiles.is_array() || rej_tiles.empty())
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "rejection artifact tiles invalid"));
  // [RELEASE-02 probe] 规模 gauge: 待积分 tile 数
  ASTROCS_PROBE_GAUGE("phase2", "integrate.tiles", static_cast<double>(rej_tiles.size()));
  for (const auto& rt : rej_tiles) {
```

## 11. `P2-skyplane-apply-tile`（可选）

- 位置：`lib/infrastructure/scheduler/src/module_adapters.cpp:4676-4678`
- 说明：可选: upm_apply 逐 tile 的 sky_plane 应用上下文 (库层 eval_block 已计时)

锚点原文（前后文即锚点首/末行）：
```cpp
    for (int t = 0; t < n_tiles; ++t) {
      const uint64_t tip = tile_ipix[static_cast<size_t>(t)];
      if (aio_hips_read_tile_f32(sig, tip, sig_buf.data()) != 0 ||
```

替换为：
```cpp
    for (int t = 0; t < n_tiles; ++t) {
      const uint64_t tip = tile_ipix[static_cast<size_t>(t)];
      // [RELEASE-02 probe] 逐 tile: sky_plane 应用 (库层 eval_block 已计时, 此处补 tile 上下文)
      ASTROCS_PROBE_SCOPE_CTX(_probe_sky_tile, "phase2", "sky_plane.apply.tile");
      ASTROCS_PROBE_TAG(_probe_sky_tile, "tile_id", static_cast<unsigned long long>(tip));
      if (aio_hips_read_tile_f32(sig, tip, sig_buf.data()) != 0 ||
```

