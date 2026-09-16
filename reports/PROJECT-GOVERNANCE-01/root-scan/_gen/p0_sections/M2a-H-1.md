## M2a-H-1 precision_mode 不参与累加域选择：核心 IR 通道恒 FP32 累加，config 要 FP64 也拿不到 float64 累积（违宪章 §5.3）

- 类别: H_NUMERIC
- 优先级: P0
- 来源: L04-001（**部分修复后定稿残余**：叶子原全称含"缺省静默降 FP32"半边，该半边在当前树已不成立——见 _merge/M2a.md §B 已修复表；本条只定稿"累加域由输入块 dtype 决定"这一残余事实）
- 位置: lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp::run_drizzle_internal（use_f64 选路段）、::hp_drizzle_run（PRECISION 头覆盖段）；lib/healpix_db/healpix_drizzle/drizzle_engine.h::DrizzleConfig::precision_mode；lib/healpix_db/healpix_drizzle/drizzle_engine.cpp::drizzleTiled / ::writeHisTilesT；lib/core/src/module_adapters.cpp::p1_op_drizzle（data 块 add_block 处）；lib/drizzle/src/module_entry.cpp::drz_execute
- 证据摘录（逐字，复核时点现文）:
  > （drizzle_engine.h:37-39）// 精度模式 (0=FP32 binary32 默认, 1=FP64 binary64) 
 uint8_t precision_mode = 0;
  > （drizzle_engine.h:60）// FP32 模式: TileAccumulatorT<float> — **真 FP32 累计**, 不共享 double
  > （hp_drizzle_api.cpp:1022-1031 区段）累加/落盘选路由 `img.use_f64`（即 "data" 块 dtype = AIO_BLOCK_FLOAT64?）决定：`img.use_f64 ? engine.drizzleTiled<double>(...) : engine.drizzleTiled<float>(...)`；`config.precision_mode` 在该选路中不出现
  > （module_adapters.cpp:2303）int rc = aio_frame_add_block(frame, "data", AIO_BLOCK_FLOAT32, im.px(), ...)   // 恒 FLOAT32，与 precision_mode 无关
  > （module_adapters.cpp:2346-2347）if (std::strcmp(kv.k, "PRECISION") == 0) return std::string(precision_mode == 1 ? "fp64" : "fp32");
  > （module_adapters.cpp:2382-2383）rc = hp_drizzle_run(frame, nside, nested, pixfrac, hiss_path.c_str(), &res, precision_mode);
  > （drizzle_engine.cpp:2099, 2133-2140）acc.pixels[local].sum_flux = static_cast<double>(tile.pixels[local].sumFlux); ... if (config.precision_mode == 1) { ... writer.add_tile_f64(...) }   // FP32 累加值加宽后按 FP64 tile 落盘
  > （docs/algorithms/DRIZZLE_GEOMETRY.md:165-168）- **B2-A12 精度 provenance（无 silent 缺省）**: 累加精度由 `drizzle.precision_mode`（整数 0=FP32 / 1=FP64）显式给出；缺失或非整数 → DATA 拒绝（不 silent 降 FP32）。
  > （ASTROCS_PROJECT_CONSTITUTION.md:44 区段 §5.3）Drizzle 采用 float64 累积、按合同输出 float32/float64，并验证能量/面亮度语义、support 和不确定度传播。
- 权威依据: 宪章 §5.3（float64 累积）、§4.2、§7.3；SCI-DRZ-001 §9:104（"FP64 累积 F_p/sumVarNum/D_p"）；ALG-DRZ-001 §8:165-168（B2-A12 精度 provenance）
- 问题说明: 当前树里"请求 FP64"与"得到 FP64 累加"之间没有任何耦合：`config.precision_mode` 只用于 (a) 帧头 PRECISION 字符串、(b) p1_stack.json 记账、(c) 落盘时把已完成的 float32 累加值 `static_cast<double>` 后写 FP64 tile / 选 HISS signal_dtype=1。真正的累加标量域由 PipelineFrame "data" 块 dtype（`img.use_f64`）决定，而核心 IR 节点 `p1_op_drizzle` 在 :2303 **恒以 AIO_BLOCK_FLOAT32** 提交图像，随后在 :2382 把用户要求的 precision_mode 原样传下去：故 `"precision_mode":1` 的请求实际是 binary32 逐元素累加 + 事后加宽落盘，累加误差按 FP32 累积而产品声明为 FP64。另一通道（lib/drizzle 的 DLL adapter，f64 manifest 真以 AIO_BLOCK_FLOAT64 提交时）能拿到真 FP64，说明这是**通道级不对称**而非全局缺省，也说明选路逻辑本可以按块 dtype 走、但 config 层没有任何一致性校验或显式拒绝（无"要 FP64 但来的是 FP32 块"的 fail-fast）。
- 影响: ① 累加域直接决定方差/信号尾数，FP32 累加在多帧大 N 场景下损失有效位，违 §5.3 的"float64 累积"发布前提；② 与 M2a-C-2 联动，产品级元数据（HISS signal_dtype、p1_stack.json precision_mode、帧头 PRECISION=fp64）对累加精度作**虚假声明**，下游按 FP64 容差比较会得出错误结论；③ 编排面与 IR 面同一 config 键语义不同，跨入口不可复现。
- 建议处置: ① `img.use_f64` 与 `config.precision_mode` 一致性校验：请求 FP64 而块为 FLOAT32 → 显式 DATA 拒绝（对齐 B2-A12 已确立的"无 silent 缺省"口径）；或 ② 让选路真正由 precision_mode 驱动（FP64 请求即 double 累加器）；③ 补回归：同一输入 pm=0/pm=1 的逐 leaf 数值差与 FP32/FP64 理论界比较（现 test_phase1_inprocess.py::test_09 只比 n_tiles/n_tiles_written，:251-252）。
- 置信度: 高（四处代码现状逐行复读：engine.h 声明、api 选路、adapters 恒 FLOAT32、engine 落盘加宽）
- related: L04-019（variance 面提前降精度，同一"元数据精度与实际精度脱钩"根因）、M2a-C-2（元数据失真半边）、L04-004（无注册回归）
