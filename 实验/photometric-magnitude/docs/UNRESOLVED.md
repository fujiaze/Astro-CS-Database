# UNRESOLVED · P1 通量积分拟合的未决登记

本文件是科学链 P1（`实验/photometric-magnitude`）的**未决事项登记落点**：每条给出
现象、复现命令、读数与初步归属，并写明所需输入。编号 `U-P1-xx` 是条款 ID，一经落定
不重排、不回收；已裁决的条款见 `DISPUTES.md`（`A-P1-xx`）。

## 条款清单

### U-P1-01 成品帧 vis 的 GC/T4 组在测光前解算失败

- **现象**：`run/RELEASE-05/vis/configs/p1_gc.json`（由 `eng/tools/e2e/make_vis_configs.py`
  生成）的每一帧在 PLATESOLVE 阶段解算失败，`p1_wcs.json` 不携带 `wcs` 对象；下游测光
  节点随即对该帧报 `PHOT_WCS_UNUSABLE`，写产品节点按「任何一帧失败都显式判红」中止运行。
  **配置段本身无错**（`grep -c "PHOT_CONFIG_" <日志>` = 0），即与曲线库/名字键无关。
- **复现命令**：
  ```
  python3 eng/tools/e2e/make_vis_configs.py --limit 1
  python3 eng/tools/monitoring/mem_guard.py --max-rss-gb 18 --timeout 3600 -- \
      build/linux-control/acsd normalize -y --json run/RELEASE-05/vis/configs/p1_gc.json
  ```
- **读数**（真实 T4 帧，`testdata/Galaxy_Center_T4/lights/panel{1,2,3}`）：
  - 进程 `exit_code=4`、`status=phase1_failed`；
  - `[wcs] DEGRADED (not blocking): frame Galaxy_Center_mosaic3_T4_… unsolved after 2
    robustness attempt(s); status=WCS_NO_DETECTED_SOURCES n_pairs=0 rms_px=0.0000
    n_detected=0`；
  - `p1_wcs.json` 不可用（`carries no 'wcs' object`）⇒ 测光节点
    `upstream_status=PHOT_WCS_UNUSABLE`、`upstream_error=… missing/invalid crval`。
- **初步归属**：**解算（PLATESOLVE）路径**，不是测光/曲线库路径。该数据集帧头**有**
  `OBJCTRA`/`OBJCTDEC`/`CRVAL1,2`/`CRPIX1,2`/`CD1_1…CD2_2`/`A_i_j`/`AP_i_j`（SIP），
  但配置用 `wcs.init_source = header_pointing` 时解算未产出 WCS（`n_detected=0`）。
- **影响**：vis 的 GC 组 p1 无法产出（帧级判红 + 运行级不发布数据集清单）；vis 的 M42 组
  不受影响（同一生成器的 M42 组实测 `exit_code=0`、`phase1 complete`）。
- **所需输入**：该数据集可用的初始指向/几何（显式 WCS 八参数或可解算的 `init_source`），
  或该组帧的点源检测/解算参数。属**数据/解算单**，本轮不修。
