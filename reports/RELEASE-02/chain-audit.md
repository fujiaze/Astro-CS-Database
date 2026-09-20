# RELEASE-02 生产链条审计（CHAIN-AUDIT）

- 任务：回答负责人「怎么这么多设计了的算法都没有接入链条，咋回事？——把目前真实链条分析一下」
- 范围：以**代码与真实产物为准**（不采信文档对现状的描述），给出①真实链条②全部缺口（A–E）③根因④修复优先级⑤门禁盲区
- 硬约束遵守：未修改任何生产代码/文档；零 git 写；未跑 ninja/cmake/ctest；临时目录 /dev/shm/astrocs_chain 已清理
- 证据目录：run/RELEASE-02/chain-audit/（phase1-chain.md / phase2-chain.md / phase3-chain.md / config-and-design-gaps.md）
- 产物基线：run/RELEASE-02/L4-rebuild/（完整 post-fix 基线 = **upmfix_out**；mosaic_out 为不完整运行）
- 行号：全部为本次审计时仓库**当前真实行号**；拿不准标「待确认」

---

## 0. 执行摘要（先看这段）

1. **真实链条是「节点链」而不是「会话链」**：astrocs normalize|mosaic|export → commands.cpp 会话分派 → runtime_client.cpp run_pipeline → build_pipeline_ir 生成 8/7/5 节点 IR → astrocs_core Runtime 调度 → module_adapters.cpp 的 p1_op_*/p2_op_*/p3_op_*。lib/phase1_session|phase2_session|phase3_session 是**遗留通道**，不在 CLI 生产路径上（CLI 直接调它们已被 check_prod_reachability.py 明令禁止）。

2. **「设计但未接入」的规模**：登记缺口 **48 项**（B 类注册但不在管线 3 项、C 类代码存在无生产调用者 7 项、D 类设计有实现缺失 24 项、E 类默认值/死键使功能失效 14 项），另有 **8 项交叉实测缺陷**（含 2 项 P0）。A 类（已接入且执行）20 个节点全部核对。

3. **最危险的不是「缺功能」，而是「静默出错」**：
   - **X-01（P0，科学输出被主动做坏）**：生产 mosaic 做 corrected=(raw−C_k)−δ_k（module_adapters.cpp:4937/4947-4954），C-DELTA 实测帧间 std/M 中位：完全不校正 13.454%、只减 C 0.131%、**减 C 再减 δ 13.974%**（比不校正还差）。即自由 C 场已把帧对齐，生产再减 δ 把它重新拉开。
   - **X-02（P0，静默不收敛）**：FIX-UPMSCALE 之后 UPM C 场复活，但生产模型实测 iterations=100, converged=0, objective=24081.46；该状态**不在 p2_upm_model.json**（只在 .bin 内嵌 JSON），p2_op_upm_fit 不调用 p2_upm_convergence。
   - **C-01/C-02（P0，Phase1 测光归一化从未执行）**：apply_photometry（photometry_apply.cpp:29）全仓非测试调用者 **0**；节点表 8 个节点无 apply 节点；注释指向的 pc_calibrate/simple 节点**不存在**（仅注释 module_adapters.cpp:3078）。全部 Phase1 产物是未测光 ADU（p1_phot.json: photometry_applied=false, photscal=1.0，49/49 帧）。

4. **「咋回事」的一句话根因**：**实现完成度 ≠ 接线完成度**。算法在自己目录里写完、自己测试全绿（且测试用合成 O(1) 参数），但**没有任何机器门**要求它必须出现在生产调用图上；同时生产配置缺键就退回代码默认（smoothing_lambda=0、cosmetic 缺键即关、algorithm_* 死键），而**工具路径**（stage2.cpp）又是另一套实现（会 /g_k、会 large_scale、smoothing auto=0.1）。两条路径越漂越远，文档又描述设计态。

5. **门禁盲区**：现有 check_prod_reachability.py 只断言「CLI 不直连禁用符号 / Runtime 未死 / ACR 不可达」，**不检查任何算法是否有生产调用者**；check_pipeline_graph.py 只比对 IR↔trace，**不比对注册表↔IR**；check_module_map.py 只到模块级，不到算法级；**没有任何 CI 门**检查「配置键是否被消费」「生产尺度参数是否使功能失效」「真实数据端到端」——L4 是人工跑的。

---

## 1. 真实链条（以代码为准）

### 1.1 命令入口 → 会话分派 → 唯一执行路径

```
build/astrocs normalize|mosaic|export --json <cfg> -y
  └─ lib/infrastructure/cli/commands.cpp
       session_dispatch(:1982)
         ├─ cmd_session1_run(:1611)  → run_with_resource_gate(:579) → run_pipeline({1}) (:678)
         ├─ cmd_session2_run(:965)   → run_with_resource_gate(:579) → run_pipeline({2}) (:678)
         └─ cmd_session3_run(:1140)  → run_with_resource_gate(:579) → run_pipeline({3}) (:678)
              └─ lib/infrastructure/cli/runtime_client.cpp
                   register_cli_modules(:22) → core::register_phase_modules
                   build_pipeline_ir(:101)  → phase1/2/3_nodes()（:130-189 / :193-222 / :226-252）
                   load_pipeline(:350) → Runtime::run(:371)
                        └─ lib/infrastructure/scheduler/src/{runtime,pipeline,scheduler,executor}.cpp
                             └─ lib/infrastructure/scheduler/src/module_adapters.cpp
                                  register_phase_modules(:7981) → p1_nodes(:7999) / p2_nodes(:8024) / p3_nodes(:8051)
                                  make_p1/p2/p3_node_module → p1_op_*/p2_op_*/p3_op_*
```

- 每个命令只跑**一个 phase**（{phase.back()-0}，commands.cpp:678）；三命令互不串（设计 §1.2）。
- 配置透传：phase_config()（runtime_client.cpp:32）把完整 doc 原样作为每个节点的 config（pdoc=doc :37），节点读的是**顶层扁平键**；仅对 V1 inputs.lights 做键名映射（phase1 :85-90，phase2 :74-79）。

### 1.2 调度执行（IR → 节点）

- IR 节点按 inputs/outputs typed artifact 建依赖边；pipeline.cpp 做静态验证（未消费产物须登记为 pipeline outputs，见 runtime_client.cpp:276-285）。
- 执行器按依赖拓扑调度，__workers 由 Runtime lease 注入（module_adapters.cpp:4527/4830/5884）。
- 旁证：check_prod_reachability.py:33-39 把 p[123]_session_(run|...)、hp_drizzle_run_hips 等列为**CLI 禁用符号**，并断言 Runtime owner 符号在二进制中可达 —— 这从机器面确认「生产 = Runtime 节点链」。

### 1.3 Phase1（normalize）8 节点与真实产品

IR：runtime_client.cpp:146-188；注册：module_adapters.cpp:7999-8008；分派：:6593-6627。

| IR node | module_id | operation / entry | 实现 | 输入 | 输出产品 | L4 实测 |
|---|---|---|---|---|---|---|
| cal | astrocs.phase1.calibration | calibrate / astrocs_phase1_calibrate_v1 | p1_op_calibrate :1583 | input_lights + master_* | calibrated_*.fts | 有 23:20:34 |
| cos | astrocs.phase1.cosmetic | cosmetic_correct / ..._cosmetic_v1 | p1_op_cosmetic :1913 | calibrated | cleaned_*.fts | 有 23:20:34 |
| psf | astrocs.phase1.star-psf | detect_sources / ..._starpsf_v1 | p1_op_star_psf :2222 (impl :2001) | cleaned | p1_sources.json, p1_psf.json | 有 23:20:39 |
| wcs | astrocs.phase1.wcs-platesolve | plate_solve / ..._wcs_v1 | p1_op_wcs :2494 | sources | <frame>/p1_wcs.json | 有 23:20:49 |
| phot | astrocs.phase1.photometry | measure_flux / ..._photometry_v1 | p1_op_photometry :2964 | psf+sources | p1_flux.json, p1_phot.json | 有 23:20:42 |
| snr | astrocs.phase1.noise-snr | estimate_snr / ..._noisesnr_v1 | p1_op_noise :3116 | fluxes | p1_snr.json | 有 23:20:46 |
| drz | astrocs.phase1.drizzle | drizzle_stack / ..._drizzle_v1 | p1_op_drizzle :3432 | calibrated+wcs+photprov | <frame>/{signal,support}, <frame>/p1_stack.json | 有 23:21-23:22 |
| wr | astrocs.phase1.writer | write_hips / ..._writer_v1 | p1_op_writer :3874 | stacked | <frame>/p1_final.json, p1_products.json | 有 23:22:07 |

- **多帧已修复**：p1_op_drizzle 现遍历全部 input_lights（:3583），逐帧 drizzle（:3588/3597/3788）、逐帧 p1_stack.json（:3797）；writer 强校验 products==inputs（:4035-4041）。实测 12 个 tag 产物数与 input_lights 一致，Σ=**49/49**（GAP_AUDIT §8.3 的 P0-21 已闭合）。
- Phase1 注册但不在 IR 的 module_id = **空集**（8==8）。

### 1.4 Phase2（mosaic）7 节点与真实产品

IR：runtime_client.cpp:197-205；注册：module_adapters.cpp:8024-8032；分派：:6809-6839。

| IR node | module_id | operation / entry | 实现 | 输出产品 |
|---|---|---|---|---|
| coverage | astrocs.phase2.coverage | compute_coverage / ..._coverage_v1 | p2_op_coverage :4271 | p2_coverage.json |
| sample | astrocs.phase2.sample | sample_frames / ..._sample_v1 | p2_op_sample :4345 | p2_samples.json |
| upm_fit | astrocs.phase2.upm-fit | fit_upm / ..._upmfit_v1 | p2_op_upm_fit :4466 | p2_upm_model.{json,bin}, p2_sky_plane.bin |
| upm_apply | astrocs.phase2.upm-apply | apply_upm / ..._upmapply_v1 | p2_op_upm_apply :4731 | p2_corrected.json + 49x.bin |
| reject | astrocs.phase2.reject | reject_outliers / ..._reject_v1 | p2_op_reject :5113 | p2_rejection.json + 4x.bin |
| integrate | astrocs.phase2.integrate | integrate_frames / ..._integrate_v1 | p2_op_integrate :5591 | p2_integrated.json + 5x.bin |
| write | astrocs.phase2.write | write_mosaic / ..._write_v1 | p2_op_write :6203 | p2_final.json + HiPS signal/support |

- 完整 post-fix 基线 = upmfix_out（astrocs_run_424d9ee81fb8.json status=complete）。**mosaic_out 缺 p2_integrated.json/.bin、p2_final.json、p2_sky_plane.bin，是未跑完的运行**，不应作验收基线。
- Phase2 注册但不在 IR：**astrocs.phase2.resample**（phase2_descriptor:606-621，注册 :7980-7985，make_session_module<P2Api>）。

### 1.5 Phase3（export）5 节点与真实产品

IR：runtime_client.cpp:229-235；注册：module_adapters.cpp:8051-8057；分派：:7853-7857。

| IR node | module_id | operation / entry | 实现 | 输出产品 |
|---|---|---|---|---|
| properties | astrocs.phase3.properties | read_properties / ..._properties_v1 | p3_op_properties :7061 | p3_props.json |
| wcs | astrocs.phase3.wcs | build_wcs / ..._wcs_v1 | p3_op_wcs :7115 | p3_wcs.json |
| resample2 | astrocs.phase3.resample2 | resample_projection / ..._resample_v1 | p3_op_resample :7162 | p3_resampled.json/.bin |
| writer | astrocs.phase3.writer | write_fits / ..._writer_v1 | p3_op_writer :7502 | output_phase3.fits, p3_writer.json |
| verify | astrocs.phase3.verify | verify_output / ..._verify_v1 | p3_op_verify :7639 | p3_verify.json |

- 实测 p3_upmfix/ 7 产品齐全；p3_verify.json reopen_ok=1/coverage_ok=1/canonical_match=true。
- Phase3 注册但不在 IR：**astrocs.phase3.resample**（phase3_descriptor:625-642，注册 :7988-7993，make_session_module<P3Api>）。

### 1.6 注册表 vs 调度（注册≠调度）

| 集合 | 数量 | 成员 |
|---|---|---|
| 冻结绑定表 module_ports.registry.json | 20 | 8 p1 + 7 p2 + 5 p3 |
| 代码注册 register_phase_modules | 22 | 上表 20 + astrocs.phase2.resample + astrocs.phase3.resample |
| 生产 IR 实际引用 | 20 | 8 + 7 + 5 |

=> 有 **2 个 module_id 注册但永不被任何 IR 调度**，且它们携带**完整的遗留 session 执行体**（P2Api/P3Api → p2_session_run/p3_session_run），构成潜在旁路。反向更严重：大量算法实现**根本没注册成节点**（见 C 类）。

### 1.7 遗留会话通道（非 CLI 生产入口）

- lib/phase1_session/p1_session.cpp（p1_session_run 仅 4 段：io_read/calibrate/cosmetic/io_write）、phase2_session/p2_session.cpp（coverage/sample/upm_build/persist）、phase3_session/p3_session.cpp。
- 证据：CLI 的 check_prod_reachability.py:34 禁止 CLI 调用 p*_session_run；make_session_module<P1Api> 全仓 0 命中（P1Api 定义 module_adapters.cpp:1044-1049 但从未注册）；lib/phase1_session/module.yaml 的 astrocs.phase1.session 未被 register_phase_modules 注册。
- commands.cpp:458 注释仍写「phase1 run: 进程内调用 p1_session」——**注释与真实路由不符**（注释级漂移）。

---

## 2. 全部「设计但未接入」清单（A–E）

### A 类 — 已接入且执行（20/20 节点，file:line 见 §1.3-1.5）

20 个节点全部有唯一真实 operation 委托，且 L4 产物实测存在。**但「已接入」不等于「科学正确」**：其中 phot 节点是「只测不应用」（C-01），upm_apply 是「双重扣除 + 不除 g_k」（X-01），integrate 是「无方差传播」。

### B 类 — 注册了但不在管线里（3）

| ID | module_id | 证据 | 影响 |
|---|---|---|---|
| B-01 | astrocs.phase2.resample | descriptor :606-621，注册 :7980-7985（make_session_module<P2Api>），IR 无 | 占位注册携带完整 p2_session_run；生产不可达，但可被外部枚举/直调 -> 潜在旁路与两套语义 |
| B-02 | astrocs.phase3.resample | descriptor :625-642，注册 :7988-7993（make_session_module<P3Api>） | 同上；P3Api 会执行完整 p3_session_run |
| B-03 | astrocs.phase1.session（module.yaml） | lib/phase1_session/module.yaml，未被注册 | 文档/合同面登记但代码注册面不存在 |

### C 类 — 代码存在但无生产调用者（双向搜索确认）

| ID | 能力 | 定义 | 全仓调用者 | 生产 module_adapters | 影响 |
|---|---|---|---|---|---|
| C-01 | **apply_photometry**（I_photo=k_photo·I_cal） | lib/algorithms/calibration/src/photometry_apply.cpp:29（声明 .h:37） | 仅 tests/.../test_photometry_apply.cpp（14 处）；未编入任何生产 target（根 CMakeLists.txt:446-450 只 4 TU；仅 tests/unit/CMakeLists.txt:789-800） | **0 命中** | **科学**：Phase1 全为未测光 ADU，缺相对流量标度；下游一切测光比较失真。量级未量化 |
| C-02 | **pc_calibrate/photometric_calib 节点**（SCI-PHOT-001） | 函数在 lib/algorithms/photometry/cpp/src/pc_api.cpp:38/178/438/563/1120/1166，仅测试 target 编译；lib/photometric_calib 目录已不存在 | 唯一生产调用点 legacy orchestrator.cpp:2812/2888，不在 CLI 路径 | 节点**不存在**（module_adapters.cpp grep photometric_calib=0；registry 无） | **科学**：C-01 的直接根因；注释 :3078 把责任指向该不存在节点 = 责任悬空 |
| C-03 | **p2_upm_ma_build**（MA 模型，可估 g_k/b_k/s） | upm.cpp:1916（声明 upm.h:305） | stage2.cpp:518（工具）、phase2_integrate.cpp:1627（v6 库，p2int:: 全仓 0 调用者）、测试 | **0 命中**；生产用 W2 p2_upm_build_geo（:4543） | **科学**：乘性光度响应 g_k 生产不生效；工具路径 (raw−C−δ)/g_k 与生产 raw−C−δ 分叉（见 X-08） |
| C-04 | **p2_large_scale_apply**（大尺度拖线掩膜） | rejection.cpp:2336（声明 rejection.h:397） | stage2.cpp:1749（工具）+ synthetic_gate.cpp:5145+（测试） | **0 命中**；large_scale.enabled 默认 0（rejection.cpp:1180，stage2_common.h:76） | **科学**：拖线掩膜生长生产不可用；TRAIL 实测其为卫星线残留的次因 |
| C-05 | p2_upm_normalized_weights | upm.cpp:1395（仍用绝对阈值） | lib 内 0 调用者（**待确认**是否测试用） | 0 | 低；与已修的 per-control 归一化路径不同 |
| C-06 | **稀疏 SNR 层**（帧级x帧内） | lib/algorithms/integration/v6/src/weight_chain.cpp:118；schema 存在 | v6 内部 | in.sparse=nullptr（module_adapters.cpp:5694 自注「尚未接入生产数据面」） | **科学**：DESIGN §3.4/§4.3 能力生产不可用；sparse_snr_layer=true 无效果 |
| C-07 | 投影 registry（v6 4 投影 / legacy 4 投影） | p3_proj_v6.cpp（registry :270-281）、p3_projection.cpp | 仅 tests（tests/unit/CMakeLists.txt:1280-1282）；p3_v6_export.cpp 不在生产库（CMakeLists.txt:686-687） | 生产仅 TAN 硬路径（p3_wcs.cpp:64） | **科学/产品**：SIN/CAR/AIT/STG/MOL/CEA/ZEA 不可用 |

### D 类 — 设计有、实现缺失（24，摘要）

完整表见 run/RELEASE-02/chain-audit/config-and-design-gaps.md §D.2。要点：

- **D-08 八投影**：设计 ASTROCS_DESIGN.md:347-352 冻结 TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA；生产 export 仅 TAN（p3_wcs.cpp:61-84），v6 registry 只 4/8（p3_proj_v6.cpp:270-281），STG/MOL/CEA/ZEA 未实现（projection/README.md:36；module.yaml:92 entrypoint=MISSING）。实测 projection=SIN -> rc=2。
- **D-09 output_mode 三值**：生产仅 surface_brightness（module_adapters.cpp:7174-7179 显式拒其它）；实测 point_source_flux rc=2。
- **D-11 独立 background 节点缺失**：IR 8 节点无 background（设计 §3.2:102）。
- **D-14 预检页缺资源/磁盘占用预估**：subcommand.h:209-232 无实现（设计 §3.5:206-208 要求）。
- **D-15 滤镜库匹配缺失**：CLI 用自由 filter_passband（:3567,3886），不加载 config/filters.json（设计 §3.3:147 要求未知滤镜 error）。
- **D-16 config/defaults.json 无生产 C++ 消费者**（设计 §3.3:142,147、§3.5:200 称其为默认值来源）；默认值实际硬编码在节点/结构体。
- **D-17/D-18/D-19 coverage/sampling/rejection 配置面缺失**：p2_op_coverage 只读 output_dir（:4274）；p2_op_sample 只读 output_dir/__workers（:4346,4371），采样参数固定 p2_sampler_default_config()；reject 对象整块零读取（仅类型校验 :6727），large_scale/rejection_classes/sigma_gate/max_iter/keep_moving_sources 不可配。
- **D-20 多目标产品族缺失**：设计 surface_brightness/point_source/psfsw 产品族；生产只写 signal/support（L4 p2_final.json:12-15）。
- **D-21 correlation_output 无消费者**。
- **D-24 v6 projection registry 非生产构建成员**。

### E 类 — 参数默认值使功能失效 / 死键（14）

| ID | 键/默认 | 生产代码 | 对照（工具/文档） | 影响 |
|---|---|---|---|---|
| E-01 | upm.smoothing_lambda 默认 **0.0** | module_adapters.cpp:4536-4537 仅显式覆盖；uc{} 零初始化；upm.h:75；upm.cpp:234 | 工具 model.smoothing="auto" -> **0.1**（stage2_common.cpp:132-140） | UPM 图平滑生产默认关闭；键路径也不同 |
| E-02 | **reject_profile 默认 astrocs_adaptive_pixel** | module_adapters.cpp:5139 | 工具/SCI/合同默认 wbpp_2_9_1（stage2_common.h:69、docs/science/REJECTION.md:21,38、DATA_SEMANTICS.md:1043）；astrocs_adaptive_pixel 在 docs/** **零命中** | 生产排异走**未登记**的 profile；L4 产物实测写入该值（mosaic_out_w1/p2_final.json:20）。**科学** |
| E-03 | algorithm_rejection_method / algorithm_weight_mode / algorithm_upm_gauge / algorithm_psf_model | 仅 CLI 白名单 parser.cpp:312,319,320 | 模板/schema/插件文档声明这些键 | 用户填键**静默忽略**；DESIGN §4.5 的 auto/显式/表达式三态不可达 |
| E-04 | reject 对象整块 | 仅类型校验 :6727 | 工具 stage2_common.cpp:214-314 | large_scale/rejection_classes/sigma_gate/max_iter/keep_moving_sources 不可配 |
| E-05 | cosmetic 键缺失 -> 校正**静默关闭** | module_adapters.cpp:1918-1924（mode=disabled 且返回 success） | CLI normalize 模板不输出 cosmetic（session_commands.h:109-128） | 照模板跑不做坏点/宇宙线校正且不报错 |
| E-06 | precision 合同 required 但 CLI 拒键 | 不在白名单（parser.cpp:275-323）；生产零读取 | schema phase_config_*.schema.json:110-121 required | 实测 unknown key precision exit 3；合同形状不可运行 |
| E-07 | CLI export 模板缺必填 output_mode | session_commands.h:161（json=nullptr）；生产必填 :7167-7171 | — | 照 --template 跑 export 必 DATA 失败；L4 是手工补键 |
| E-08 | sparse_snr_layer no-op | in.sparse=nullptr（:5694） | schema/模板有该键 | 键无效 |
| E-09 | frame_gain / large_scale 工具实现、生产零接线 | module_adapters.cpp 0 命中 | stage2_common.h:64,76；stage2.cpp:455-562 | 见 C-03/C-04 |
| E-10 | config/defaults.json 不被读取 | lib 内仅注释/测试 | DESIGN §3.3:142,147 | 默认值非配置单一来源 |
| E-11 | config/filters.json 未知滤镜校验缺失 | filter_passband 自由字符串 | DESIGN §3.3:147 | 拼错滤镜不报 error |
| E-12 | coverage/sampling 科学键无消费者 | :4274 / :4346,4371 | 插件 09/10 文档 | 用户不可调 |
| E-13 | sky_plane.enabled 默认 true 但注释写「缺省关闭」 | :4625 true；stage2_common.h:53 注释 | — | 注释误导（非功能缺陷） |
| E-14 | legacy_allow_weight_fallback 保留但已无成功降级路径 | :5625,5728 | — | 死键 |

### X 类 — 交叉实测缺陷（不属于 A–E，但必须登记）

| ID | 缺陷 | 证据 | 影响 |
|---|---|---|---|
| **X-01** | **δ_k 双重加性扣除**：corrected=(raw−C_k)−δ_k | module_adapters.cpp:4937/4947-4954；C-DELTA 实测（reports/RELEASE-02/c-delta-ruling.md:71-72，本次只读复跑 which_correction.py 逐行复现）：raw 13.454% / raw−δ 2.799% / **raw−C 0.131%** / **raw−C−δ 13.974%** | **P0 科学**：接缝首要驱动项；比完全不校正还差 |
| **X-02** | **UPM 不收敛且状态不可见** | 实测 upmfix_out/p2_upm_model.bin 内嵌 JSON：iterations=100, converged=0, objective=24081.46（tolerance=1e-6 绝对判据不可达）；p2_upm_model.json **不含** iterations/converged/objective；p2_op_upm_fit 不调 p2_upm_convergence（upm.cpp:1280） | **P0/P1 科学+可观测**：C 场虽复活但未收敛，且验收面看不到 |
| **X-03** | cosmetic 疑似空操作 | 实测 calibrated_*.fts 与 cleaned_*.fts sha256 **完全相同**（t2_m1_red，12/12 tag 同） | 待确认（可能确为 0 缺陷；但 hot_fixed/cold_fixed 未持久化，无法判定） |
| **X-04** | p1_flux 端口声明 ELECTRON，实现 ADU 且无 gain 换算 | registry:80 / desc:812 vs photometer 实现 | typed 单位与真实产物不一致 |
| **X-05** | writer 的 typed 输出 fits(DATA-P1-FITS) 未落任何 .fts | p1_op_writer 只写 p1_final.json/p1_products.json（:3972-4054） | IR 端口与真实产品不符 |
| **X-06** | IR typed 端口不忠实真实文件依赖 | resample2 只声明 wcs_plan 实读 props+hips；upm-apply/integrate 多输入未连边，靠 output_dir 文件约定 | 调度无时序险（线性链），但静态图不能证明真实数据流 |
| **X-07** | mosaic_out 是不完整运行 | 缺 p2_integrated/p2_final/p2_sky_plane | 不应作验收基线（完整基线 = upmfix_out） |
| **X-08** | **工具路径与生产路径公式分叉** | 工具 stage2.cpp:1070/1443：(raw−C−δ)/g_k；生产 :4939-4942：raw−C−δ，注释明写「不除 g_k」 | 同一设计两条实现，产物不可比 |

### 缺口统计

| 类 | 数量 | 备注 |
|---|---|---|
| A 已接入且执行 | 20 节点 | 全部核对 |
| B 注册但不在管线 | 3 | 2 个占位携带遗留 session |
| C 代码存在无生产调用者 | 7 | 含 apply_photometry、MA、large_scale、稀疏 SNR |
| D 设计有实现缺失 | 24 | 见 config-and-design-gaps.md §D.2 |
| E 默认值/死键使功能失效 | 14 | 含 reject_profile、smoothing_lambda、cosmetic |
| X 交叉实测缺陷 | 8 | 含 2 个 P0（X-01/X-02） |
| **合计登记缺口** | **56** | B+C+D+E = 48 项「设计但未接入」类；X 为实测缺陷 |

---

## 3. 根因分析（回答「咋回事」）

### 3.1 假设验证

**H1 节点表/注册表与管线脱节（注册≠调度）—— 成立（部分）**
- 证据：注册 22 vs IR 引用 20（B-01/B-02）；但**主因是反向**——大量算法实现（C-01..C-07）**根本没注册成节点**，不是「注册了没调度」。冻结绑定表 module_ports.registry.json 只有 20 项，恰好等于节点集，说明**绑定表只覆盖已接线的节点**，不覆盖设计算法全集；没有任何门要求两者相等。
- 判词：**「实现完成度 ≠ 接线完成度」** 是主模式；「注册≠调度」只解释 B 类 3 项。

**H2 配置默认值静默关闭功能 —— 成立（强）**
- 证据：E-01..E-14；尤其 E-01（smoothing=0）、E-02（reject_profile 未登记值）、E-05（cosmetic 缺键即关）、E-03（algorithm_* 死键）。生产配置（L4 mosaic_49.json）只有 5 个键，其余全走代码默认。
- 判词：**缺键->代码默认** 是系统性静默失效通道。

**H3 「由某某节点承担」责任悬空 —— 成立**
- 证据：module_adapters.cpp:3077-3080 注释称 I_photo=k_photo·I_cal「由 pc_calibrate/simple 类节点承担」，而该节点**全仓不存在**（C-02）；module_adapters.cpp:644-646 注释称工厂委托 P3Api session adapter，与现状 :8062 矛盾；commands.cpp:458 注释称走 p1_session，与实际 IR 路由矛盾。
- 判词：**注释承担了不存在的责任**，无人核对。

**H4 测试用合成参数（O(1)）掩盖生产参数（~1e-22）失效 —— 成立（强，有实测）**
- 证据：synthetic_gate.cpp:73 control_ivar=1.0；生产 control_ivar 中位 **5.595e-22**、per-control Σ 中位 5.101e-21、**Σ>1e-12 的 control = 0**。旧绝对门 sums>1e-12 在合成档恒真、在生产恒假 -> C 场空操作（FIX-UPMSCALE 报告 §2）。synthetic_gate.cpp:531 现已加注释承认此裂缝，但**该回归只在算法自己的测试里，不在 CI 门**。
- 判词：**尺度裂缝**是「测试全绿但生产失效」的直接机制。

**H5 工具路径与生产路径双实现漂移 —— 成立（强）**
- 证据：公式 /g_k（stage2.cpp:1070/1443）vs 不除（module_adapters.cpp:4939-4942）；默认 smoothing auto=0.1（stage2_common.cpp:140）vs 0；large_scale/frame_gain 工具有、生产 0 命中；两套配置词表（model.smoothing vs upm.smoothing_lambda；algorithm_* vs 裸键）。
- 判词：**同一设计两条实现**，且生产与工具产物不可比。

**H6 门太弱（合成档、纯数学档，不覆盖生产尺度/真实数据）—— 成立（强）**
- 见 §5。核心：没有「算法->生产调用者」门；没有「注册表↔IR」门；没有「配置键被消费」门；没有生产尺度门；L4 真实数据 E2E 是人工跑，不是 CI 门。

### 3.2 最危险的排序（静默失效 x 无门禁 x 影响科学输出）

| 排名 | 项 | 为什么最危险 |
|---|---|---|
| 1 | **X-01 δ_k 双重扣除** | 主动把输出做坏（13.974% > 13.454%），且 sky_plane_applied=true 让它看起来「校正生效」；无任何门测帧间一致性 |
| 2 | **C-01/C-02 Phase1 测光归一化从未执行** | 全部 Phase1 产物未测光；photometry_applied=false 虽如实登记但无门判红；下游接缝/测光全被污染 |
| 3 | **X-02 UPM 不收敛不可见** | C 场复活后仍未收敛（converged=0），验收面 JSON 看不到；可能长期「看起来正常」 |
| 4 | **E-02 reject_profile 未登记默认** | 排异走文档不认的 profile；科学行为与 SCI 冻结不一致 |
| 5 | **E-01/E-03/E-04/E-05 配置静默失效** | 用户显式配置被忽略或缺失即关；无 warn |
| 6 | **C-03/C-04/C-06/D-08 能力缺失** | g_k / 大尺度掩膜 / 稀疏 SNR / 7 投影缺失；有实测影响（卫星线残留、接缝） |

---

## 4. 修复优先级（危险度 x 修复成本）

### P0（科学输出已受损；低-中成本；须负责人裁决科学变更）

| # | 修复 | 成本 | 预期 | 依据 |
|---|---|---|---|---|
| P0-1 | **去掉双重加性扣除**：只保留一次（保留 C 还是 δ 须在 Phase1 归一化修好后实测；现测 raw−C=0.131% 远优于 raw−δ=2.799%，但 C 含代偿成分） | 低（改一处公式 + provenance） | 帧间差 13.974% -> ~0.13% | X-01；C-DELTA §2 |
| P0-2 | **把测光归一化真正接到像素**：补 apply_photometry 节点（或按设计在 drizzle 前应用），让 photappl/photscal 如实落元数据；重跑 L4 normalize | 中（新节点 + 上游拟合链 C-02） | Phase1 产物进入测光坐标系 | C-01/C-02；GAP_AUDIT §9.29 |
| P0-3 | **UPM 收敛可见 + 判据改相对**：把 iterations/converged/objective 写进 p2_upm_model.json，p2_op_upm_fit 调 p2_upm_convergence；tolerance 绝对 1e-6 -> 相对 | 低 | 验收面可见；避免静默不收敛 | X-02；C-DELTA §Q5 |

### P1（危险高、成本中；多为「接线/登记」）

| # | 修复 | 成本 |
|---|---|---|
| P1-1 | reject_profile 生产默认与 SCI/合同对齐（wbpp_2_9_1）或正式登记 astrocs_adaptive_pixel | 低 |
| P1-2 | 死键 fail-closed：algorithm_* / reject 对象 / sparse_snr_layer / precision 要么实现、要么在预检报「未实现键」 | 低-中 |
| P1-3 | 接 p2_large_scale_apply 到生产 reject op 并给 large_scale 配置面（科学默认变更需 claim） | 中 |
| P1-4 | 接 g_k（或正式撤回并同步工具路径）—— 现在工具 /g_k、生产不除，必须统一 | 中 |
| P1-5 | cosmetic 缺键改为 fail-closed 或默认启用并在 manifest 明示 | 低 |
| P1-6 | 删/降级 B-01/B-02 占位注册（携带遗留 session 的潜在旁路） | 低 |

### P2（能力补全；成本中-高；多为设计范围）

| # | 修复 | 成本 |
|---|---|---|
| P2-1 | 八投影生产 registry 分派（先 TAN/SIN/CAR/AIT，再 STG/MOL/CEA/ZEA） | 高 |
| P2-2 | 稀疏 SNR 层接生产（in.sparse） | 中 |
| P2-3 | 多目标产品族（surface_brightness/point_source/psfsw） | 高 |
| P2-4 | config/defaults.json 成为默认值单一来源；滤镜库校验 | 中 |
| P2-5 | coverage/sampling 配置面；预检资源/磁盘预估 | 中 |
| P2-6 | 修 X-04/X-05/X-06 typed 端口与真实产品不一致 | 低-中 |

---

## 5. 门禁盲区与建议

### 5.1 现有门做了什么（已核）

| 门 | 位置 | 覆盖 | **盲区** |
|---|---|---|---|
| CHK-001 check_prod_reachability.py | tools/quality/check_prod_reachability.py | CLI 不直连 p*_session_run/hp_drizzle_*（:33-39）；Runtime owner 在二进制（:116-119）；ACR 不可达（:122-125）；compile_commands 源存在（:128-135） | **完全不检查「算法是否有生产调用者」**。它建了可达图（:163-204）却不断言算法覆盖 -> C-01..C-07 全部漏过 |
| CHK-002 check_pipeline_graph.py | tools/quality/check_pipeline_graph.py | IR↔observed trace 节点/边/单位/哈希一致 | 不比对「注册表↔IR」-> B-01/B-02 漏过 |
| CHK-MODULE-MANIFEST check_module_map.py | tools/quality/check_module_map.py | 23 插件模块 7 项齐备、facade/no-op 识别、产品清单 | 只到**模块级**（23 篇），不到**算法级** -> apply_photometry/MA/large_scale 漏过 |
| CHK-003 check_serial_heavy.py | tools/quality/check_serial_heavy.py | 并行轴/lease/串行热点 | 不涉算法接线 |
| 算法自测（ctest） | lib/algorithms/coverage/tests/synthetic_gate.cpp 等 | 合成档 O(1) 参数、纯数学档 | **不覆盖生产尺度**（control_ivar=1.0 vs 5.6e-22）；不覆盖配置键消费 |
| CHK-E2E-REPRO | ci/checks.json:7061 | WCS closure 复现（closure_metric.py selftest） | **不是** normalize->mosaic->export 真实数据全链 E2E；L4 是人工跑 |
| ci/checks.json grep | — | smoothing/large_scale/algorithm_rejection_method/apply_photometry/g_k **0 命中** | 这些关键能力**无任何 CI 检查项** |

### 5.2 本应被抓到但没抓到的缺口 -> 建议补的门

| 建议门 | 抓什么 | 怎么实现（最小） |
|---|---|---|
| **CHK-ALGO-WIRING**（最高价值） | 每个「设计声明的算法」必须有生产调用者，否则必须在 DORMANT/NOT_IMPLEMENTED 台账登记并带权威引用 | 扩展 check_prod_reachability.py：对 lib/algorithms/** 导出的公开符号，用 nm 判其在 build/astrocs 中可达，或在 docs/.../DORMANT_LEDGER.yaml 有登记；无登记即 FAIL。负例：把 apply_photometry 从生产移除 -> 必红 |
| **CHK-REGISTRY-IR-PARITY** | 注册表 module_id 集合 vs 生产 IR module_id 集合 | 解析 register_phase_modules 的注册表与 runtime_client.cpp 的 IR 节点表，双向差集必须为空或差集项带 lifecycle=DORMANT + 权威引用。抓 B-01/B-02 |
| **CHK-CONFIG-CONSUMED** | CLI 白名单/config_registry 里每个键必须有 >=1 个生产 reader（或显式标 output-only/help） | 静态扫描 parser.cpp kSessionKeys 与 module_adapters.cpp 的 value()/contains()；无消费者即 FAIL。抓 E-03/E-04/E-08 |
| **CHK-PROD-SCALE** | 关键内核在生产参数尺度下不失效 | 把 FIX-UPMSCALE 的生产尺度回归（control_ivar=5.6e-22）纳入 CI；对 UPM/weight-chain/rejection 各加一个「生产尺度」用例（能红能绿） |
| **CHK-PROVENANCE-CONSISTENCY** | 产物↔manifest↔provenance 一致，且「降级/未应用」必须判红或至少 WARN | 断言 p1_phot.json photometry_applied=false 时下游必须 PHOTDEGRADE=1 且不得声明 RELATIVE_FLUX；断言 p2_corrected.json sky_plane_applied 与帧间一致性指标挂钩；断言 p2_upm_model.json 含 converged |
| **CHK-REALDATA-E2E** | normalize(多帧)->mosaic(多帧)->export 真实数据全链 | 把 L4 的 run_l4.sh 产品数闸门（49/49）与帧间一致性阈值做成 CI 检查（可 heavy profile）；当前 CHK-E2E-REPRO 不覆盖 |
| **CHK-CONFIG-DEFAULTS** | 代码默认 vs 工具/文档默认一致 | 把 smoothing_lambda（0 vs 0.1）、reject_profile（astrocs_adaptive_pixel vs wbpp_2_9_1）等做成三方对照门，不一致即 FAIL 或需登记裁决 |

---

## 6. 诚实边界与待确认

1. **X-03 cosmetic 空操作**：calibrated 与 cleaned sha256 相同（12/12 tag）——可能是该数据确无坏点/宇宙线，也可能是校正未生效；hot_fixed/cold_fixed 未持久化，**待确认**。 **已确证（2026-09-20）**：§9.44「009/010/016 cosmetic 节点恒传 nullptr ⇒ 检测永久禁用却报 ok」（`工程控制/RELEASE-02/GAP_AUDIT.md:1284`）⇒ 判为「校正未生效」，非「该数据确无坏点」。旧文 =「待确认」。
2. **E-02 astrocs_adaptive_pixel**：是否为负责人裁决的生产默认？docs/** 零命中，工具/SCI 默认 wbpp_2_9_1，**待确认**。 **已确证（2026-09-20）**：§9.40 C2「C2，**我自研的**」（`工程控制/RELEASE-02/GAP_AUDIT.md:1116-1121`）⇒ `reject_profile` 以 `astrocs_adaptive_pixel` 为准。⚠ 但「生产默认」部分已被 **SD-18**（`ACCEPTANCE.md:117`）收回：低 n 档走保守路径（`n≤3→none`），该方法保留为显式 opt-in。旧文 =「待确认」。
3. **p2_upm_normalized_weights（C-05）** 是否仅测试用，**待确认**。
4. **D-01 两套模板**（schema 形状 vs CLI flat）哪套是目标态，**待确认**。 **已确证（2026-09-20）**：§9.56 前台裁决 4「**以 CLI/schema 为准，订正最高设计示例**」（工程控制/RELEASE-02/GAP_AUDIT.md:1910-1911）。⚠ **冲突登记（以更晚裁决为准）**：ACCEPTANCE.md:109 SD-13 记「以设计 §3.3 嵌套形态为准」，而 §9.68（:2611-2642）定 normalize 配置为**多数据块 JSON**（顶层 blocks[] + 平铺单块简写），并**否决** phase_config_normalize.schema.json / normalize.phase_config.json 的逐帧 inputs[] 形态 ⇒ 本项目标态应以后续 §9.68 为准，需前台复核。旧文 =「待确认」。
5. **D-16 defaults.json** 是否计划运行时读取（设计说读取，现状零消费者），**待确认**。
6. 本报告**未运行** build/astrocs 全链重跑（仅只读运行 CLI 模板/校验，及读取既有 L4 产物）；X-01 的数值引自 C-DELTA 报告并由 phase2 子代理只读复跑 which_correction.py 逐行复现，未在本审计内独立重算全部 29336 个 control。
7. 卫星线/接缝的最终数值验收以负责人裁决后的重跑为准；本审计只做链条与接线事实。

---

## 附：证据索引

- 本报告子报告：run/RELEASE-02/chain-audit/phase1-chain.md、phase2-chain.md、phase3-chain.md、config-and-design-gaps.md
- 既有决定性报告：reports/RELEASE-02/c-delta-ruling.md（X-01）、reports/RELEASE-02/fix-upmscale-report.md（UPM 阈值）、reports/RELEASE-02/fix-gk-report.md（方案 B）、reports/RELEASE-02/fix-sky-report.md（sky_plane rc=6）、工程控制/RELEASE-02/GAP_AUDIT.md §9
- 真实产物：run/RELEASE-02/L4-rebuild/{norm,upmfix_out,p3_upmfix,logs}
- 门禁代码：tools/quality/check_prod_reachability.py、check_pipeline_graph.py、check_module_map.py、check_serial_heavy.py；ci/checks.json
