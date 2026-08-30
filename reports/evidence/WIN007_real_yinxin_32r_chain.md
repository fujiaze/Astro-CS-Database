# WIN-007 银心 32R — 32帧全链运行(当前SHA b842899)

> 机器: Fatduck Windows, astrocs.exe(win_rel/Release 当前SHA b842899)。真实银心 T4 数据。
> 32R 定义(03 任务详情/控制包): 冻结候选 SHA/profile/config/32R manifest; 只启动一次; first-10s gate; 全 Phase | 32/32 contribution; run manifest/hash 完整。

## 帧集(32张 Red 帧)
- 分布: `Galaxy_Center_T4/lights/panel{1,2,3}\*-180S-Red.fts` → 11(panel1)+11(panel2)+10(panel3)=32 帧(同 panel 相邻帧保证重叠)。
- 帧名 `Galaxy_Center_mosaic{1,2,3}_T4_flying_dutchman-20250702/16/18@*.fts`, 4500×3600, 180S。
- masters(4500×3600): `masterBias_BIN-1_4500x3600`, `masterDark_BIN-1_4500x3600_EXPOSURE-180.00s`, `masterFlat_BIN-1_4500x3600_FILTER-Red_mono`。

## 全链结果: PASS(32/32 contribution)
| 阶段 | 结果 | run_id |
|---|---|---|
| phase1 | `calibrate ok: 32 frames`, exit0 | `529854867a21` |
| per-frame drizzle (nside=2048, 每帧独立HiPS) | HIPS_PATHS=32 | — |
| phase2 | `sample ok: obs=529 overlap_controls=47`, **exit0**, 32 hips_paths | `4f885b8b8fd4` |
| phase3 | `phase3 complete`, **exit0**, output_phase3.fits | `4e81f2e0dc96` |

- **32/32 contribution**: HIPS_PATHS=32 (32 帧各成独立 nside2048 HiPS), phase2 n_inputs=32 → 529 观测 / 47 重叠控制(UPM 有足够 overlap 可解)。
- phase3 产出 `output_phase3.fits`(32777 KB) — 科学视图(candidate)可加载。

## 结论
- **WIN-007 银心 32R 全链(phase1→逐帧drizzle→phase2→phase3) PASS, 32/32 contribution**。
- 遗留: 冻结 32R manifest(候选SHA/profile/config/帧集) + first-10s gate + 资源/内存门控 + WIN-008 seam/flux/coverage 数值门槛。
- 记录不宣称 release。
