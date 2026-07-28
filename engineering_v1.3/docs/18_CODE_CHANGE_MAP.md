# 代码修改地图

预计重点代码面：

- `lib/plate_solve/cpp/ipv/`：标准 WCS/SIP 导出和匹配对诊断；
- `lib/orchestrator/`：阶段数据、CLI 事件、provenance 和真实进度；
- `lib/photometric_calib/`：投影/唯一匹配/诊断，不承担坐标补丁；
- `lib/snr_estimator/` 与 Drizzle adapter：完整 WCS/SIP 和 HISS SNR；
- `lib/calibration/`：T1–T4 元数据解析和 Master resolver；
- `lib/astro_image_io/`：HCSD 持久读句柄、批量 leaf API、兼容测试；
- `lib/healpix_db/healpix_stack/`：重叠图、梯度指标、真实 SNR²、进度拆分；
- `lib/healpix_db/healpix_browser_qt/`：异步 Tile、LRU、GPU Renderer、性能 trace。

任何模块修改前先由相应任务冻结接口；避免 Orchestrator、Photometric 和 Drizzle 各自实现一套 WCS 变换。
