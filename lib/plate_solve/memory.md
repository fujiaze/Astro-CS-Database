# plate_solve - 模块开发memory

## 模块职责
天文图像plate solving（IPV算法，相对向量法），基于Gaia DR3SP星表完成WCS+SIP坐标变换求解，输出CD矩阵、CRVAL/CRPIX、SIP系数及求解质量指标。

## 当前版本
- 版本号：V4.30
- 最新commit：9dafd79c
- 更新时间：2026-07-12

## GitHub仓库
- 仓库地址：https://github.com/fujiaze/PlateSolve-IPV-Cpp
- 默认分支：main

## 依赖列表
- C++17, OpenMP
- astro_image_io.dll（PipelineFrame命名块容器 + FITS读写）
- ipv_solver.dll（IPV求解器，依赖astro_image_io.dll + gaia_client.dll + star_detector.dll）
- gaia_xpsd_client（Gaia DR3SP星表C客户端）
- star_detector.dll（Moffat4星点检测 + 饱和星检测）
- GaiaDR3SP数据库目录

## 关键决策记录
- **IPV算法**：基于Valdes 1995三角形匹配 + iter_trans多项式拟合，相对向量法实现星表-图像配对
- **相对向量法**：通过三角形边长比构造不变量，避免绝对坐标依赖，提升鲁棒性
- **ipv_solve_from_memory内存接口**：直接接收PipelineFrame数据指针，无临时文件落盘，性能更优
- **命名块容器接口**：pipeline_adapter.py使用get_block_data("data")/kv_set("header",...)/add_block("star_det",...)，输出star_det块(FLOAT32[N,4]: x,y,flux,mag)与gaia_cat块(FLOAT64[N,3]: ra,dec,mag)
- **SIP双向系数完整写入**：修复旧版只写前向SIP(A/B)丢弃逆向SIP(AP/BP)的问题，避免astropy WCS边缘退化

## 进度日志
### 2026-07-12 ipv_solve_from_memory内存接口完成
- 完成ipv_solve_from_memory内存接口实现，RMS=0.1431"
- pipeline_adapter.py重写为命名块容器版，废弃旧版get_pixels()/set_wcs()/set_sip()
- 新增star_det块与gaia_cat块输出，供下游模块使用
- 新增_gaia_cone_search_for_solver()调用gaia_client C API查询星表
- RA/DEC解析支持"HH MM SS.S"和"HH:MM:SS.S"两种格式
- 推送至GitHub：commit 9dafd79c

### 2026-07-11 SIP AP/BP逆向系数写入修复
- 修复write_wcs_to_fits()只写前向SIP(A/B)的问题
- 补全AP/BP写入，pipeline_adapter同步注入
- 修复astropy wcs_world2pix vs all_world2pix差异：wcs_*前缀不应用SIP，all_*前缀才在Python层应用SIP修正

### 2026-07-13 仓库结构整理完成
- GitHub仓库分支统一为main
- 文档刷新并重新推送
- 最新commit: 3a0db4a6

### 2026-07-27 P11-002 WCS真实星对闭环诊断工具建立 (v1.2 engineering)
- 工具位置：engineering_v1.2/evidence/P11-002/scripts/wcs_closure_diagnostic.py
- 工具独立性：完全独立于 PlateSolve internal transform（不导入 to_astropy_wcs，不读 wcs_result.cd/crval/crpix/sip_*）
- 仅用 astropy.wcs.WCS 从 FITS header 构建 WCS 做 pixel↔sky 转换
- 30/30 单元测试 PASS（含 5 项工具独立性硬约束）
- 在 T3_LUM_NGC55 + T2_HA_LDN43 两帧代表帧运行：
  - T3: PlateSolve RMS=0.151px(31pairs) vs 独立 median=0.897px(702 matched) — 5.9× 差距
  - T2: PlateSolve RMS=0.108px(33pairs) vs 独立 median=0.772px(1237 matched) — 7.2× 差距
- astropy WCS 数值闭环精度 1e-10 px（完美）
- 真实残差分布：
  - T3 Y方向主导 (0.848 vs 0.218 px)
  - T2 X/Y均衡 (~0.5 px each)
  - Q4 象限偏多（两帧一致）
  - SIP_ORDER=3 两帧一致
- VERDICT: PASS，为 P11-003 全帧复现提供工具基础

### 2026-09-07 P1-WCS-DOC 模块合同冻结（控制包 wave W1）
- 合同三件套落位本目录：README.md r1 重写（取代 V4.30 营销式旧 README，
  旧性能指标叙述保留于 GitHub 上游与上文存档）+ module.yaml（11 号标准
  §4，CONTRACT_READY，entrypoint=MISSING）+ 本节。
- 合同 ID：SCI=SCI-WCS-001（docs/science/ASTROMETRY.md 共享引用不改）；
  ALG=ALG-WCS-001（PLATESOLVE.md §11 逐符号锚，12 导出+内核符号链实测）；
  DATA=DATA-P1-WCS（DATA_SEMANTICS §18）；API=API-WCS-001（PUBLIC_API，
  12 导出符号锚）；SRC=SRC-WCS-001；TEST=TEST-WCS-DESIGN-001（§11.4
  冻结容差，可执行 TEST-P1-WCS-001 归 P1-WCS-TEST）。
- ID 修正记录：descriptor 占位 alg_id=ALG-002/api_id=API-P1-004 为编排层
  词汇（module_adapters.cpp:450-464），真实合同 ID=ALG-WCS-001/API-WCS-001
  由本任务注册；SCI 行由占位 SCI-P1-WCS-001 修正为既有 FROZEN SCI-WCS-001
  （P1-PHOT 先例，不改 docs/science）。
- DISP-WCS-001..006 登记不改码（PLATESOLVE.md §11.3）；核心为 DISP-WCS-001
  （hunt R1）：CD 矩阵退化静默坍缩至 CRPIX 无错误通道——域内实例
  wcs_tan.cpp:48-51（零日志）、域外同族 wcs_transform.cpp:39-49（仅
  stderr）；失败-置信度语义冻结"退化必须 success=0 禁止冒充解"。
- 像素中心双契约登记：统一契约（star_measurements index-is-center）↔
  IPV 接口契约（center=index+0.5），orchestrator.cpp:1867 显式桥接；
  CRPIX 1-based 与 +0.5 自洽。
- 验收：traceability checker rc=0 warns=4（与基线逐条一致，无新增）、
  pytest traceability 全过、contract_graph rc=0、doc_index rc=0、
  selfcheck ALL PASS、lib/**.c/.h/.cpp/CMakeLists 与 docs/science 零 diff。
- 遗留：ALG-WCS-002（旧 TST-WCS-INV/FAIL 设计词汇）未在本节逐符号展开
  （域内唯一生产 ALG=ALG-WCS-001，见 §11.5 状态声明）；构建/CMake/取消点
  整改归 P1-WCS-IMPL；descriptor ID 对齐归 P1-WCS-INT。
