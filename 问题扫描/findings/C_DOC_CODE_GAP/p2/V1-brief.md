# V1 定稿 · P2 五项（同源与诊断面）

### V1-N-03 snr_estimator.h 接口文本自斥：同名字段两值
- 类别 C_DOC_CODE_GAP ｜ P2 ｜ OPEN ｜ :463 称 snr_phot/median_snr「均置 1.0」而 :465-466 字段注释写「= median(SNR_F)」；实现 :639 置 median_snr、而 snr_estimate 路径 :146/:318 用 1.0 ⇒ 同名两值。related V1-N-04、簇1 注释与合同互斥。

### V1-N-05 DATA-P1-FLUX 单位三口径未收敛
- 类别 C_DOC_CODE_GAP ｜ P2 ｜ OPEN ｜ photometry registry:43 已改 ADU、noise-snr registry:33 仍 ELECTRON、实现 module_adapters.cpp:764,782 仍 ELECTRON ⇒ 同一 DATA-ID 三处两种单位。related A-03、M2b。

### V1-N-06 v1 导出未初始化新字段且退化哨兵三套不一（含 N-06b 计数缺口）
- 类别 C_DOC_CODE_GAP ｜ P2 ｜ OPEN ｜ snr_extract_model(v1) 只初始化 5 个旧字段（:570-577），新增 median_source_snr/frame_depth_flux5_adu/frame_depth_m5_mag 在三条早退路径（:581/:588/:627）**保留调用方栈值**；v2/v3 有 memset(0) 但头 :470-472 承诺「无 ZP 时 NaN」⇒ 哨兵 NaN/0.0/0 三套。N-06b：:1119-1121 用 support 当权重但不加计数，与 :576「diagnostics 标红」不符。related §6.3、M3-A-002。

### V1-N-07 唯一钉退休口径的测试仍存在但因不在根图而永不运行 ⇒ 两条被改路径树内无活测试
- 类别 F_TEST_GAP ｜ P2 ｜ OPEN ｜ lib/snr_estimator/test/test_snr_estimator.py:63-112 仍钉 snr_phot=1/(LN10*sigma)=4.3429/8.6859 与稠密图 SNR=snr_phot*(40/40)，与新实现（稠密图=IDW(SNR_F)、退化填 1.0）逻辑互斥且未改；它不红只因需 snr_estimator.dll（不在根图）+ sys.path 指向不存在的 lib/snr_estimator/python ⇒ snr_estimate/snr_estimate_f64 两条被改路径在树内无任何活测试。related V1-N-01、M8-F-004、C-02。

### V1-N-11 逐源重建轮廓的串行 CPU 重算风险（§17.6/§10.5 候选，数值需实测）
- 类别 G_GOV_GATE ｜ P2 ｜ OPEN ｜ snr_science.cpp:104-140 对每源重建 (2*max(30,ceil(12*FWHM))+1)^2 轮廓（FWHM>=21.4px 封顶 256 ⇒ 513^2≈2.63e5 元素≈2.1MB/次）+ 每源 std::vector；snr_frame_science.cpp:104-112 逐源串行；根 CMakeLists.txt:495-501 astrocs_phase1_noise 无 OpenMP；registry execution_class=cpu_heavy；sum_p2 只依赖 FWHM 却无记忆化 ⇒ 单线程长计算与利用率风险（数字需运行期实测，V1 未跑）。related M5a-G-002、A-39、§17.6。
