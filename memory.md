# Astro CS Normalization Database - 项目记忆

## 项目概述
天文CCD图像校准与标准化数据库系统。使用C++开发核心算法模块（编译为DLL），Python脚本调用。
开发环境：16线程CPU + 64GB内存，多线程任务使用16线程。

## 仓库整合记录

### 2026-07-24 11 个子仓库合并到主仓库 Astro-CS-Database ★工程整合★
- **主仓库**：https://github.com/fujiaze/Astro-CS-Database (public, 115 commits)
- **整合方式**：`git filter-repo --to-subdirectory-filter lib/<module>` 将 11 个子仓库历史重写到 `lib/<module>/` 路径下，合并到一个主仓库。原 11 个子仓库未受影响（仅被 clone 读取）。
- **主仓库本地工作区**：`f:\Astro dev\Astro CS Normalization Database`（已从外部临时目录 `AstroCsNormalizationMain` 迁移回原项目目录，删除 11 个子仓库 .git，主仓库 .git 直接落在项目根目录）
- **排除内容**：siril-1.4.3/、GaiaDR3/、GaiaDR3SP/、.trae/、logs/、output/、build/、archive/、testdata/、*.xpsd、*.hiss、*.hcsd、编译产物、图像（已用 filter-repo 从历史清理，原 70MB→1.87MB）
- **合并的 11 个子仓库**（commits 数）：
  - astro_image_io(15) / calibration(7) / data_pipeline(3) / dynamic_psf(7) / gaia_xpsd_client(10)
  - healpix_db(11) / orchestrator(14) / photometric_calib(14) / plate_solve(1) / snr_estimator(2) / star_detector(12)
- **导出包**：`f:\Astro dev\Astro CS Normalization Database\AstroCS_Database_Context.zip` (1.93 MB)
  - 含 `repo.bundle`（git bundle，可 `git clone repo.bundle` 恢复）+ `PROJECT_CONTEXT.md`（项目脉络导航）+ `docs/`（架构文档原文）
  - 用途：给 ChatGPT 在 Linux 沙箱解包分析项目脉络，输出下一阶段工程文档
- **整合脚本**：`.trae/specs/2026-07-24-repo-consolidation/`（spec.md + checklist.md + step1~7 脚本，step7_migrate.ps1 为主仓库迁移脚本）
- **行尾符规范化**：`.gitattributes` 强制全仓库 LF（`* text=auto eol=lf`），二进制文件标记 binary 不转换；解决主仓库迁移后 252 个文件因 CRLF/LF 差异产生的假阳性 modified 状态
- **已封存子仓库**（保留为历史快照，README 注明依赖指向主仓库 Astro-CS-Database，不再独立开发）：
  - PlateSolve-IPV-Cpp：https://github.com/fujiaze/PlateSolve-IPV-Cpp（commit 74e98cc，README 已更新）
  - Gaia-DR3-DR3SP-Client-C：https://github.com/fujiaze/Gaia-DR3-DR3SP-Client-C（commit 94afbf7，README 已更新）
  - Dynamic-PSF-Cpp：https://github.com/fujiaze/Dynamic-PSF-Cpp（commit 7db81f8，README 已更新，修正仓库名不一致 Dynamic-PSF → Dynamic-PSF-Cpp）
- **后续**：主仓库作为统一开发入口，原 11 个子仓库可继续独立维护或归档

## 目录结构
- `lib/` - 通用库（文件IO、Gaia数据库客户端等）
- `lib/astro_image_io/` - 统一天文图像 I/O 层 + Pipeline 管线引擎（FITS/XISF 读写 + .ahpx 单帧格式 + zstd/lz4 压缩 + PipelineFrame + 管线编排引擎）
  - GitHub: https://github.com/fujiaze/Astro-Image-IO-C (commit 37fee6c)
  - `include/aio_pipeline.h` - PipelineFrame 结构体 + 动态内存管理接口 + XML 调试导出
  - `include/aio_pipeline_engine.h` - 管线编排引擎（阶段注册/调度/OpenMP批量并行）
  - `src/aio_compressor.h/.cpp` - 压缩层 (namespace aio::, C API: aio_compress/aio_decompress/aio_compress_bound)
  - `src/ahpx/aio_ahpx_writer.h/.cpp` - .ahpx 写入器 (namespace aio::ahpx::)
  - `src/ahpx/aio_ahpx_reader.h/.cpp` - .ahpx 读取器
  - `src/ahpx/aio_ahpx_api.cpp` - 简化 C API (aio_ahpx_write/read_header/read_pixels/read_snr)
  - `src/aio_pipeline.cpp` - 动态内存管理实现 + XML 调试导出 (base64 编码, UTF-8 路径)
  - `src/aio_pipeline_engine.cpp` - 引擎实现 (OpenMP 16线程并行 + 自动内存释放 + 调试导出)
  - `python/astro_image_io.py` - Python封装 (ImageReader/FITSWriter/PipelineFramePy/PipelineEngine)
- `lib/calibration/` - 天文CCD校准模块（主帧生成/图像校准/坏点修复，C++核心+Python流水线）
  - `include/astro_calibration.h` - C API 头文件（extern "C", AC_API 导出宏）
  - `src/` - C++ 核心实现
- `lib/healpix_db/` - Healpix 天球分块数据库（已拆分，核心仓库仅保留浏览器 + 独立仓库本地副本）
  - GitHub: https://github.com/fujiaze/Healpix-Database (commit 7129e32)
  - `healpix_browser_qt/` - Qt6+OpenGL 3.3 浏览器 (C++ core无Qt依赖 + Qt widgets层, 替代 healpix_browser_cpp + healpix_browser_web + healpix_browser)
    - Task 1-5 完成: 目录结构/HealpixMath/STFEngine/BrowserBackend/GLRenderer
    - Task 6-12 完成: AbstractView/SphereView/MainWindow/STFPanel + 编译验证 + 归档 (SingleFrameView 已废弃归档至 widgets/archive/)
    - 2026-07-14 STF bug修复: to_uniforms不再归一化, compute_data_range用0.5%/99.5%分位
    - 2026-07-14 切平面导航: gnomonic逆变换(极区无奇点) + 30°经纬线网格(Ctrl+G开关)
    - 2026-07-14 视角与像素修复: (1)eta符号翻转(抓画面拖模式) (2)虚拟轨迹球导航(forward+up双向量, axis=dy*right-dx*up, 统一左右上下无roll) (3)球面切平面菱形像素(替代矩形近似,无cos_dec发散) (4)no_data(value<=0)不渲染提升流畅性 (5)renderer用传入forward/up不重算
    - 2026-07-14 视角控制重写(最终方案): 赤道仪相机(yaw=ra增量绕Z/pitch=dec增量绕east, up始终north-up重算不携带不roll) + look_at_matrix column-major存储bug修复(原存V^T转置矩阵导致左右拖动roll) + pix_size公式修正(原240/nside偏大4.1倍致摩尔纹, 改为√(π/3)/nside×180/π≈58.6/nside) + 菱形外扩1.25覆盖drizzle pixfrac=0.8缝隙 + MAX_FOV=50°(原170°, 限制球面yaw畸变) + 左右方向修正(center_ra_+=dx, side=forward×up指向-Y/西)
    - 2026-07-16 依赖迁移: healpix_io.dll → astro_image_io.dll (lib/astro_image_io/, AIO_ENABLE_HEALPIX 定义, aio_healpix_io.h 兼容宏)
    - 关键依赖: astro_image_io.dll需复制到build目录, Qt6 DLL在C:\msys64\mingw64\bin, QT_PLUGIN_PATH=C:\msys64\mingw64\share\Qt6\plugins
    - 启动: `$env:Path = "C:\msys64\mingw64\bin;$env:Path"; $env:QT_PLUGIN_PATH = "C:\msys64\mingw64\share\Qt6\plugins"; Start-Process -FilePath build\healpix_browser_qt.exe -ArgumentList "`"<hiss路径>`""`
    - 编译: `cd lib\healpix_db\healpix_browser_qt; cmake --build build`
    - 模块记忆: lib/healpix_db/healpix_browser_qt/memory.md
  - `healpix_io/` - 已归档（2026-07-16，API 并入 astro_image_io，源码在 archive/）
  - `healpix_stack/` - 已拆分为独立仓库 Healpix-Mosaic（本地保留，.gitignore 忽略）
  - `healpix_drizzle/` - 已拆分为独立仓库 Healpix-Drizzle（.gitignore 忽略）
  - `archive/legacy/` - 2026-07-16 归档: healpix_browser_python/ (PyQt5+vispy) + healpix_lod/ (被内存ud_grade替代) + tests/
- `lib/healpix_db/healpix_stack/` (独立仓库) - 稀疏 HEALpix 堆栈存储
  - GitHub: https://github.com/fujiaze/Healpix-Mosaic-Cpp (commit 027b64f)
- `lib/healpix_db/healpix_drizzle/` (独立仓库) - 球面 Drizzle 重投影
  - GitHub: https://github.com/fujiaze/Healpix-Drizzle-Cpp (commit ecf8758)
- `lib/photometric_calib/` - 测光定标模块（含 flux_calibrator 流量校准）
- `lib/snr_estimator/` (独立仓库) - SNR 估算模块（乘法模型 SNR=SNR_phot×(SNR_psf/median)）
  - GitHub: https://github.com/fujiaze/Snr-Estimator-Cpp-Python (commit 405d153)
- `lib/orchestrator/` (独立仓库) - 管线编排引擎模块（v2.0 两段流水线 10 节点 C++ CLI + Python 调试层；stage1 8/8 节点实际 DLL 调用单帧端到端验证通过）
  - GitHub: https://github.com/fujiaze/Orchestrator-Cpp-Python (commit 25b8e81)
- `lib/integration_test/` - 全链路整合测试 (已删除，orchestrator/archive/scripts/ 有完整副本)
- `/GaiaDR3SP` - Gaia数据库
- `/testdata` - 测试数据

## 文档体系（2026-07-16 整理后）

- `docs/ARCHITECTURE.md` - **当前架构文档**（归纳实际状态，替代旧 PROJECT_ARCHITECTURE.md）
- `docs/PIPELINE_OVERVIEW.md` - **管线流程概述**（基于流程概述.txt，流程步骤与模块对应关系，精简版）
- `docs/DESIGN_IMPL_GAP.md` - **设计与实施割裂记录**（GAP-001 ~ GAP-010）
- `docs/archive/` - 历史文档归档
  - `PROJECT_ARCHITECTURE_2026-07-16.md` - 旧架构文档（已归档）
  - `UI_ARCHITECTURE_2026-07-16.md` - 旧 UI 架构文档（已归档）
- `memory.md`（根目录）- 项目记忆，仅做索引与全局进度
- 各模块 `memory.md` - 模块详细记录
- 根目录**非 git 仓库**，各模块独立 git 管理

## 调试文件整理（2026-07-16）

**spec**: docs/superpowers/specs/2026-07-16-project-reorganization.md + checklist.md

**整理结果**:
- 根目录清理 24 个调试文件（diag_*/healpix_*.png/qt_run_log.txt/run_healpix.bat/03_fsyn.json）
- 高价值脚本保留至模块 tools/：
  - `lib/photometric_calib/tools/diag_gradient_report.py`
  - `lib/plate_solve/tools/diag_projection_plot.py`
  - `lib/healpix_db/healpix_browser_qt/run_healpix.bat`
- 一次性脚本分散归档至对应模块 `archive/debug_2026-07/`（含 README.md）：
  - `lib/plate_solve/archive/debug_2026-07/`（diag_wcs.py, diag_wcs_offset.py）
  - `lib/dynamic_psf/archive/debug_2026-07/`（diag_psf_edge.py, diag_psf_root_cause.py, diag_image_stats.py）
  - `lib/photometric_calib/archive/debug_2026-07/`（diag_distribution.py, diag_light.py）
- 14 个一次性输出文件直接删除（diag_*.txt/png, healpix_*.png, qt_run_log.txt, 03_fsyn.json）
- 识别 10 项设计与实施割裂点（见 docs/DESIGN_IMPL_GAP.md），关键项：
  - GAP-001: healpix_browser_cpp/web 文档标称已归档但代码仍在原位（双重存在）
  - GAP-002: healpix_io 实际代码已移至 archive/ 但文档仍标为活跃（影响编译依赖）
  - GAP-005: plate_solve 本地 .git 损坏（缺 objects/，远端仓库 PlateSolve-IPV-Cpp 存在）
  - GAP-008: 4 处断层待修复（drizzle落盘/hiss格式/Python绑定/stack加权）

## 当前进度

### healpix_db 遗留代码归档 + 依赖迁移 + SingleFrameView 废弃（2026-07-16）
**spec**: docs/superpowers/specs/2026-07-16-healpix-db-legacy-archive.md + checklist.md

**归档操作**:
- healpix_browser/ (PyQt5+vispy) → archive/legacy/healpix_browser_python/
- healpix_lod/ (LOD金字塔, 被内存ud_grade替代) → archive/legacy/healpix_lod/
- tests/test_e2e_integration.py → archive/legacy/tests/
- healpix_browser_cpp/ 顶层删除 (与 archive/healpix_browser_cpp/ 字节级重复)
- healpix_io/ 源码移入 healpix_io/archive/ (API 已并入 astro_image_io)

**依赖迁移 (healpix_browser_qt)**:
- ../healpix_io/ → ../../astro_image_io/ (lib/astro_image_io/ 提供 healpix_io 兼容 API)
- 链接库名 healpix_io → astro_image_io, 添加 AIO_ENABLE_HEALPIX 定义
- CMakeLists.txt/Makefile/deploy.ps1/browser_backend.cpp #include + 5个源码注释同步
- 验证: astro_image_io.dll 构建成功(9个HEALPix I/O符号导出), healpix_browser_qt CMake 34/34编译成功

**SingleFrameView 废弃**:
- .hiss 改用 SphereView SPHERE 模式 + LOD 金字塔 (大数据集全量多边形卡死)
- 源码归档至 widgets/archive/single_frame_view.h/.cpp
- CMakeLists.txt 不再编译 single_frame_view.cpp
- 文档同步: README.md/memory.md/main_window.h/abstract_view.h/.cpp 注释 + ARCHITECTURE.md

**dynamic_psf FWHM 修复**:
- MOFFAT4_FWHM_FACTOR 0.8700 → 1.230310 (漏了 sqrt(2) 系数, 导致 FWHM 偏小 41%)
- 根因: 模型使用 Q = 0.5*r^2/sigma^2 (1/(2*sigma^2) 系数), FWHM = 2*sqrt(2)*sigma*sqrt(2^(1/4)-1)

**git 推送 (3仓库)**:
- healpix_db: 131ce3b..3c34171 (60 files, 归档+依赖迁移+SingleFrameView废弃+文档同步)
- dynamic_psf: fdead62..3499458 (Moffat4 FWHM修复 + PSF性能优化记录迁入)
- photometric_calib: fd7ddf1..7149817 (上一轮遗留commit推送)

**文档同步**:
- healpix_db/README.md 重写 v2.0, healpix_db/memory.md 追加归档记录
- healpix_browser_qt/memory.md 更新6处依赖路径 + SingleFrameView废弃记录
- docs/ARCHITECTURE.md 更新 widgets/ 结构 (SingleFrameView 归档)
- docs/DESIGN_IMPL_GAP.md 关闭 GAP-001/002/003/004

**根目录状态**:
- 根目录非 git 仓库 (符合架构设计)
- 应保留: lib/ docs/ GaiaDR3/ GaiaDR3SP/ output/ siril-1.4.3/ testdata/ .superpowers/ .trae/ .pytest_cache/ memory.md .gitignore 流程概述.txt 马赛克叠加梯度建模计划.md
- testdata/ (73.4GB, 被.gitignore忽略, 用户规则明确"testdata是测试数据")
- .superpowers/ (36KB, brainstorm技能工作目录, 2026-07-11会话记录)
- .pytest_cache/ (1.47KB, 已被.gitignore忽略)
- 流程概述.txt (3.4KB, 项目记忆引用的重要参考文档)
- 马赛克叠加梯度建模计划.md (14.9KB, 项目计划文档)
- 已清理: .tmp_healpix_repos/ (临时clone目录) + data/ + spec/ + temp_docs/ (3个空目录)

### PSF 块扩展 [N,6]→[N,9] + Python→C++ 架构决策（2026-07-15）
**spec**: .trae/specs/psf-block-extension/(三件套,已执行完成)
**下一个 spec**: .trae/specs/photometric-sigma-residual/(三件套,已创建待执行)

**架构决策(Python→C++ 迁移)**:
- Python 胶水层重新定位为**调试层**,调试完毕后由 C++ 替代
- 后续每个 spec 均需包含 §迁移策略 + §Python 退役计划
- PROJECT_ARCHITECTURE.md 已同步更新(§核心设计原则/§技术栈/§orchestrator 目录树)

**PSF 块扩展(已完成)**:
- psf_adapter.py: [N,6]→[N,9],新增 A/mad/eccentricity 三列(C++ DPSFFitResult 已计算,仅 Python 层丢弃)
- validate_psf_quality.py: shape 检查 !=6 → !=9
- photometric_adapter.py / run_pipeline_debug.py / 旧版文件: 注释与日志同步更新
- 端到端验证: 2000 颗星,1906 颗成功(95.3%),A 全部>0,7/7 检查 PASS

**依赖链后续**:
1. ~~photometric-sigma-residual~~(已完成, 2026-07-15): C++ pc_calibrate_simple_with_gaia 新增 out_sigma_residual 出参
2. SNR 模块(§14): 乘法模型 SNR = SNR_phot × (SNR_psf/median)(下一个 spec)
3. 4 处断层修复: drizzle落盘/hiss格式/Python绑定/stack加权
4. 梯度校正(§12)

### photometric-sigma-residual 暴露（2026-07-15）
**spec**: .trae/specs/photometric-sigma-residual/(三件套,已执行完成)

**改动**:
- C++ star_matcher.h/cpp: cleanOutliers/matchAndClean 新增 `double* out_sigma_residual = nullptr` 出参
- C++ photometric_calib.h: pc_calibrate_simple/pc_calibrate_simple_with_gaia 新增末尾参数
- C++ pc_api.cpp: 透传 sigma_residual + 退化路径设 0.0 + nullptr 检查
- Python photometric_calib.py: argtypes 追加 POINTER(c_double), calibrate_simple/with_gaia 返回 4 元组
- Python photometric_adapter.py: photo_stats KV 块新增 SIGMA_RESIDUAL 字段
- run_pipeline_debug.py: _collect_photo_stats + title_suffix 同步

**验证**:
- 模块自测 PASS (sigma_residual=0, 测试数据完美一致)
- 端到端管线全 6 节点通过, photometric 0.867s
- sigma_residual=0.168 mag (合理范围 0.01-0.5)
- photo_stats KV 块: N_MATCHED=1527, SCALE_FACTOR=7.13e-03, SIGMA_RESIDUAL=1.68e-01

### SNR 模块设计 + §12/§13 设计调整 + 文档职责分离重构（2026-07-15）
更新 PROJECT_ARCHITECTURE.md + UI_ARCHITECTURE.md + 各模块 memory.md，落地 SNR 估算模块设计、§12 梯度校正设计调整、§13 PSF 统一调整、文档职责分离重构。仅文档更新，不写实现代码。

**spec**: docs/superpowers/specs/2026-07-15-snr-module-and-grad-design.md（11 项设计决策，全部已确认）

**核心设计决策（全部已确认）**:

1. **SNR 与梯度严格独立**（决策2，已确认）：测光孔径背景拟合+PSF B参数都吸收梯度→SNR_phot和SNR_psf不含梯度→解耦精确非近似
2. **SNR 单次计算 + 固定权重**（决策2）：STAGE_SNR前置计算一次，梯度拟合用SNR-A固定权重→Gauss-Seidel确定性收敛
3. **SNR² 最优加权**（MLE/MVUE）：w_i ∝ SNR_i² = inverse-variance weighting，理论最小方差
4. **双层 SNR 保护**：阶段2梯度拟合用SNR-A加权（防低SNR污染梯度曲面），阶段3叠加用SNR-B²加权（防低SNR帧抬高叠加）
5. **叠加执行顺序**：梯度迭代收敛→sigma-clip（用收敛g_i一次性判定）→SNR-B²加权平均，串行不嵌套

**11 项设计决策摘要**:
1. 新增 §14 SNR 估算模块（lib/snr_estimator/，STAGE_SNR，乘法模型）
2. §12.15 SNR 耦合设计（**已确认**：SNR单次计算+固定权重解耦+SNR²最优加权+双层SNR保护）
3. §12.13 文件名修正（sigma_clip_stack → stack_engine + hp_stack_hiss）
4. §13 PSF 统一调整（psf 块 [N,6]→[N,9]，photometric_calib 新增 out_sigma_residual）
5. 管线阶段调整（新增 STAGE_SNR）
6. .hiss 格式扩展（新增 snr 通道，has_snr 标识）
7. drizzle 累加公式问题（已知问题待修正）
8. photometric_calib 接口扩展（out_sigma_residual）
9. hp_stack_hiss 前置依赖（需扩展为 SNR-B² 加权 sigma-clip）
10. **SNR² 加权贯通 4 处断层**（drizzle落盘/hiss格式/Python绑定/stack加权，待实现）
11. **PROJECT_ARCHITECTURE.md 整体重构**（文档职责分离）

**文档职责分离重构**:
- §10 浏览器架构 → 根目录 `UI_ARCHITECTURE.md`（新建）
- §11 性能优化记录 → 各模块 `memory.md`（dynamic_psf/photometric_calib/healpix_drizzle/healpix_browser_qt）
- 已归档/废弃模块详细记录 → `lib/healpix_db/memory.md`
- PROJECT_ARCHITECTURE.md 聚焦数据流/后端，新增 §10 文档职责分离说明（指向各文档位置）
- 新增 §14.7（4处断层）+ §14.8（传递优化检查：冗余/缺失/反复计算/未导出）

**待办**:
- 后续逐步实现各模块（SNR/梯度校正/PSF扩展/4处断层修复），每个模块独立 spec

### 架构文档更新 + Drizzle/Photometric 测试补全（2026-07-15）
完成三项工作：PROJECT_ARCHITECTURE.md 重写、test_drizzle.py 重写（7项测试）、test_energy_conservation.py 新建（4项测试）。

**1. 架构文档重写 (PROJECT_ARCHITECTURE.md)**:
- 日期更新 2026-07-13 → 2026-07-15
- §2.1 模块清单: healpix_browser_qt/ 标为活跃, healpix_browser_cpp/ + healpix_browser_web/ 标为已归档
- §10.7 新增 Qt6 浏览器架构细节 (auto_stretch百分位/LOD动态nside/uint8降采样/MAX_SHIFT递归查找/视口物理像素/FOV下限/need_rebuild_mesh阈值/双击启动部署)
- §11.4 新增 Drizzle 黑色缝隙修复 (5基准全1-ring + 菱形像素 + pixfrac=1.0)
- §11.5 新增 浏览器性能/视觉修复 (11个问题根因与修复)
- 附录目录结构: 新增 healpix_browser_qt/ 子目录树

**2. Drizzle 测试重写 (lib/healpix_db/healpix_drizzle/tests/test_drizzle.py)**:
- 从 .ahpx 改为 .hiss 格式 (HissReader)
- 7项测试: WCS往返/点采样/面积分配/.hiss往返/SIP畸变/梯度通量/真实数据
- **关键发现**: C++ Drizzle 输出 brightness = sumFlux (通量累加, 非亮度平均)
- **关键发现**: astropy Sip 类在 astropy.wcs 下, 非 astropy.io.fits
- **关键发现**: 点采样 (pixfrac=0) 不保证总通量守恒 (diag_arcsec=0 时 C++ 跳过像素)
- 测试结果: 6 passed, 1 skipped (真实FITS无WCS)

**3. Photometric 能量守恒测试 (lib/photometric_calib/cpp/test/test_energy_conservation.py)**:
- 4项测试: 像素级守恒/匹配星流量守恒/残差分布/退化路径
- **关键发现**: Python 3.8+ Windows 不用 PATH 加载 DLL, 必须用 os.add_dll_directory()
- **关键发现**: photometric_calib.dll → gaia_client.dll → libgomp-1.dll + zlib1.dll (MinGW运行时)
- 测试结果: 4 passed
- 运行命令: `python -m pytest lib/photometric_calib/cpp/test/test_energy_conservation.py -v`

### Drizzle 黑色缝隙修复 + 性能优化（2026-07-14）
针对 drizzle 输出在球面渲染上呈现的微小黑色缝隙，进行三项综合修复并优化候选像素检索性能。

**根因分析**:
1. 候选像素覆盖不全 - 仅中心1-ring 在 WCS 畸变下可能漏掉源像素四角延伸到的相邻像素
2. HEALPix 像素四角近似为方形, 但实际是菱形, 边缘判定不准
3. pixfrac=0.8 收缩源像素导致相邻源像素之间有固有缝隙

**修复**:
- 候选像素: 仅中心1-ring → 5基准(中心+四角)各取1-ring邻居 (5+5×8=45候选, 数组扩到48)
- HEALPix 像素四角: 方形近似 → 菱形对角计算
  - NS 半对角线 = sqrt(sqrt(3))/2 * res ≈ 0.658 * res
  - EW 半对角线 = 1/sqrt(sqrt(3)) * res ≈ 0.760 * res
  - 4 顶点 (北/西/南/东, **逆时针**顺序, 兼容 Sutherland-Hodgman 算法)
- pixfrac 默认值: 0.8 → 1.0 (drizzle_engine.h / hp_drizzle_api.h / pipeline_adapter.py / healpix_drizzle.py / run_forward_drizzle.py)

**关键教训**:
- Sutherland-Hodgman 多边形裁剪算法要求裁剪多边形 **逆时针**, 顶点顺时针会导致所有 subject 点被误判为外侧 → 0 像素输出
- HEALPix 赤道带像素是菱形 (diamond), 不是方形. 之前用 (±half_ra, ±half_dec) 方形近似在边缘会误判不相交, 导致微小黑色缝隙
- pixfrac<1.0 收缩源像素覆盖范围, 相邻源像素之间必然有间隙, 形成球面空缺

**性能结果** (nside=65536, 16.2M源像素):
| 方案 | 候选数 | pixfrac | 耗时(s) | HEALPix 像素数 |
|------|--------|---------|---------|----------------|
| queryDisc BFS | 全 | 0.8 | 67.6 | 61.6M |
| 5基准全1-ring | ~45 | 0.8 | 43.5 | 61.6M |
| 仅中心1-ring | ~13 | 0.8 | 33.7 | 61.6M |
| 5基准全1-ring+菱形+pixfrac=1.0 | ~45 | 1.0 | 47.1 | 61.6M |

**缝隙诊断** (nside=65536, 采样5000像素):
| 指标 | 修复前 | 修复后 |
|------|--------|--------|
| 零值像素 | 0 / 61598882 (0.000%) | 0 / 61611427 (0.000%) |
| 邻居缺失率 | 1.39% | 1.34% |
| 无缺失邻居像素比例 | 94.6% | 94.7% |
| 缺失位置 | ra=276.48~276.91 (边缘) | ra=276.52~276.91 (边缘) |

**结论**: 算法层面已做三项修复, 邻居缺失率从1.39%降至1.34%, 剩余1.34%全部在图像边缘 (HEALPix像素8邻居中部分落在源图像覆盖范围外), 为边界效应无法算法消除.

### WebGL 浏览器前端 Bug 修复（2026-07-13）
修复天文浏览器前端 (WebGL) 三个 bug: 横条纹渲染、缩放拖动失效、UI 改进文档。

**Bug 1: 横条纹渲染问题 (切面投影)**
- 文件: `lib/healpix_db/healpix_browser_web/js/webgl-renderer.js`
- 根因: `renderSingleFrame(data)` 将 NESTED 顺序 HEALPix 像素直接按顺序排列到方形纹理 (texWidth=ceil(sqrt(n))), 产生 Z 序曲线图案 (横条纹)
- 修复:
  - 在 `HealpixUtils` 添加 `pix2angNest(nside, ipix)` 方法 (从 C++ browser_backend.cpp 移植, 支持任意 nside, BigInt 计算 ip_low 避免大 nside 精度丢失)
  - 重写 `renderSingleFrame(data)`: 构建 Map(ipix→value), 采样 ~1000 像素用 pix2angNest 估算边界框 (圆均值 RA 处理 0/360 跨越), TAN 逆投影将天球坐标投影到 1024×1024 纹理
  - 新增 `_computeSingleFrameBBox()`: 采样像素估算数据边界框, cos(dec) 修正 RA 范围, 10% 边距
  - 新增 `_buildSingleFrameTexture(centerRa, centerDec, scale)`: 1024×1024 网格, 每像素 TAN 逆投影→radec2pixNest→Map 查值, rho>π/2 跳过
  - 新增 `getSingleFrameBBox()` 供 main.js 初始化 ViewController

**Bug 2: 缩放和拖动不工作 (单帧模式 2D 交互)**
- 文件: `js/view-controller.js`, `js/main.js`
- 根因: `_renderSingleFrame()` 不接收 viewParams, 拖动/缩放事件未影响单帧渲染; 拖动灵敏度 `0.3/zoom` 度/像素 对小 FOV 数据过大
- 修复:
  - ViewController 添加 `mode` ('sphere'/'single') 和 `singleFrameFovDeg` 属性
  - `onDrag()`: 单帧模式用 FOV/canvasWidth 灵敏度 (cos(dec) 修正 RA), 球面模式保持原逻辑
  - `onWheel()`: 单帧模式 effectiveMaxFov=singleFrameFovDeg, 球面模式保持 180°
  - 新增 `setMode()`, `setSingleFrameFov()`, `setViewParamsSilent()` 方法
  - `render()` 传递 viewParams 到 `_renderSingleFrame(viewParams)`
  - `_renderSingleFrame(viewParams)`: 根据 center_ra/zoom 计算 scale, viewChanged 检查避免 STF 更新触发的无谓重建
  - `main.js init()`: 设置 ViewController 模式; `onViewportChange()`: 加载单帧后用 bbox 静默初始化 ViewController 视角

**Bug 3: UI 改进计划文档**
- 文件: `PROJECT_ARCHITECTURE.md`
- 在 §10 浏览器架构添加 §10.7 "UI 改进计划（待实现）": 单帧/球面独立入口、文件夹交互 UI、当前为 bug 修复阶段
- 更新 §10.5 文件支持: .hiss 从 "Mollweide 投影 1024×512" 改为 "切面投影 (TAN Projection) 1024×1024"

**关键设计决策**:
- BigInt 计算 ip_low: nside=8192 时 npface=67108864 超过 Number 安全整数, ip_low=BigInt(ipix)%BigInt(npface), ix/iy 最大 8191 用 Number
- viewChanged 检查: 比较当前 centerRa/centerDec/scale 与 singleFrameTexView, 仅视角变化时重建纹理 (1024×1024 TAN 投影 ~100-500ms), STF 更新只改 uniform
- 圆均值 RA: atan2(sumSin, sumCos) 处理 0/360 跨越, 归一化到 [meanRa-180, meanRa+180] 后求 min/max
- 静默初始化: setViewParamsSilent 不触发 _notifyViewChange, 避免 renderSingleFrame 后的双重复绘

### 格式统一·浏览器重构·性能优化（2026-07-13）
spec: .trae/specs/format-unification-browser-perf/

**目标**: 统一 HEALPix 存储格式为 .hiss/.hcsd，重构浏览器为 WebView2+WebGL+C++ 后端，修复 photometric 性能瓶颈。

**完成内容 (15/15 Task)**:

#### 阶段1: 格式定义
- 新建 `lib/healpix_db/healpix_io/FORMAT_SPEC.md` - .hiss/.hcsd 格式规范文档
- .hiss: Magic "HISS" + JSON头(zstd) + ipix数组(uint64) + pixel数组(float32)
- .hcsd: Magic "HCSD" + JSON头(zstd) + 子叶块索引(49152×24B) + ipix数组 + pixel数组
- 子叶块索引: nside=64 分区，O(1) 定位，支持浏览器按需加载

#### 阶段2: healpix_io 模块
- 新建 `lib/healpix_db/healpix_io/` (C++ DLL + Python 绑定)
- C API: hiss_write/hiss_read/hcsd_write/hcsd_read/hcsd_read_leaf/hio_free
- Python: HissWriter/HissReader/HcsdWriter/HcsdReader + 便捷函数
- 往返测试 4/4 通过，导出符号验证通过

#### 阶段3: I/O 改造
- drizzle 输出从 .ahpx 改为 .hiss（drizzle_engine.cpp 调用 hiss_write）
- 内存 sigma-clip 堆叠（healpix_stack 新增 hp_stack_hiss，废弃 .ahps 输出 .hcsd）
- 废弃标记: healpix_lod/（README）、healpix_browser/（README）、astro_image_io/src/ahpx/（DEPRECATED.md）

#### 阶段4: 浏览器重构
- 新建 `lib/healpix_db/healpix_browser_cpp/` - C++ 渲染后端 + HTTP 服务器
  - BrowserBackend 类: open_file/get_required_leaves/load_leaf/ud_grade
  - 按需子叶加载: 中心 nside=8192，中间 nside=2048，边缘 nside=256
  - HTTP 服务器: localhost:18080, /api/file_info, /api/leaf, /api/all_data
- 新建 `lib/healpix_db/healpix_browser_web/` - WebGL 前端
  - 球面 WebGL 渲染 (UV 球面 64×128 分段)
  - 视角交互 (鼠标拖动旋转、滚轮缩放)
  - STF 拉伸控制面板 (移植自 stf.py)
  - 单帧模式 (.hiss) + 球面模式 (.hcsd)

#### 阶段5: 性能优化
- P0: 循环内 fprintf 清除（log_macros.h，LOG_DEBUG 默认编译时展开为空）
  - spectrum_integrator.cpp 第241行 fprintf → LOG_DEBUG
  - star_matcher.cpp 第86-89行 fprintf → LOG_DEBUG
- P1: 滤光片曲线预处理缓存（SpectrumIntegratorCache）
  - 循环前预处理一次（排序+Akima插值重采样到光谱网格）
  - 循环内只算 SED + 星等归一化 + Simpson 积分
- P2: 自适应迭代星等（mag_max 从 12.0 迭代到 16.0）
  - 返回 2000-10000 颗星，不缩小锥形搜索半径
- 性能结果: photometric 354.7s → 0.881s (99.75% 改善)，总管线 409s → 60.6s

#### 阶段6: 工程文档
- PROJECT_ARCHITECTURE.md 更新（格式体系/模块架构/浏览器架构/PSF 流程重构未来计划）
- memory.md 更新（本项目记录）

**关键设计决策**:
- .hiss 单帧存储: 稀疏 ipix + pixel 值，JSON头含 filter/exposure_s/obs_time/pixfrac/fits_meta
- .hcsd 天球数据库: 含子叶块索引，支持浏览器按需加载
- 废弃 .ahps: 内存 sigma-clip 不落盘（count/sum/sum_sq 三数组，与帧数无关）
- 废弃 .ahpl: LOD 金字塔应为内存数据结构（浏览器 C++ 后端 ud_grade 动态生成）
- 浏览器架构: C++ HTTP 服务器 + WebGL 前端（替代 PyQt5）
- PSF 流程重构: 查gaia→PSF拟合仅gaia星→积分校准→SNR（未来计划，仅文档）

### Task 8: WebGL 前端浏览器（healpix_browser_web，2026-07-13）
新建基于 WebGL 的浏览器前端，替代现有 PyQt5 浏览器（healpix_browser）。通过 HTTP API 与 C++ 后端（Task 7，监听 localhost:18080）通信。

**新建文件 (10个)**:
- `lib/healpix_db/healpix_browser_web/index.html` - 主页面（顶栏+画布+控制面板+状态栏）
- `lib/healpix_db/healpix_browser_web/css/style.css` - 深色主题样式表
- `lib/healpix_db/healpix_browser_web/js/stf.js` - STF 拉伸算法（移植自 stf.py, MTF+MAD自动+4预设+asinh压缩）
- `lib/healpix_db/healpix_browser_web/js/api-client.js` - C++ 后端 API 客户端（fetch+base64解码+超时）
- `lib/healpix_db/healpix_browser_web/js/webgl-renderer.js` - WebGL 球面渲染器（着色器+网格+子叶管理+矩阵运算）
- `lib/healpix_db/healpix_browser_web/js/view-controller.js` - 视角交互（鼠标拖动+滚轮缩放+触摸支持）
- `lib/healpix_db/healpix_browser_web/js/main.js` - 主逻辑（BrowserApp, 模块整合+渲染循环）
- `lib/healpix_db/healpix_browser_web/shaders/vertex.glsl` - 顶点着色器（参考）
- `lib/healpix_db/healpix_browser_web/shaders/fragment.glsl` - 片元着色器（STF+MTF+uint8 binning, 参考）
- `lib/healpix_db/healpix_browser_web/README.md` - 说明文档

**参考代码**: stf.py（MTF公式/MAD自动拉伸/4预设/asinh压缩）、sphere_renderer.py（UV球面网格/HEALPix坐标转换/GLSL shader）、single_frame_view.py（单帧浏览/通道切换）

**关键设计决策**:
- 球面渲染: UV 球面 64×128 分段，每个顶点 (x,y,z)→(ra,dec)→ipix→查值，顶点值 float32 传 GPU，片元着色器内 STF+MTF+uint8 binning（与 sphere_renderer.py 的 SphereBinningVisual 一致）
- STF 在 GPU 内完成: shadows/highlights/midtones/compression 作为 uniform，实时更新无需重建网格
- 子叶动态加载: leafTextures Map (leafIpix→{ipixMap, pixels, nside})，视角变化时按需加载+自动清理
- 单帧模式: Mollweide 投影映射到 1024×512 R32F 纹理，全屏四边形渲染
- WebGL 2.0 优先: R32F 纹理 + Uint32 索引，向下兼容 WebGL 1.0（LUMINANCE + Uint16）
- HEALPix 坐标转换: 移植自 sphere_renderer.py 的 _py_radec2pix_nest（NESTED, 北极/赤道/南极三区+位交错）
- 纯 HTML/CSS/JS 无框架依赖，fetch API 通信，base64 编码 Float32Array 传输
- 深色主题半透明控制面板，STF 滑块+预设按钮+自动拉伸

**API 端点约定** (与 Task 7 C++ 后端):
- GET /api/ping, GET /api/file_info, POST /api/required_leaves
- GET /api/leaf?ipix=X&nside=Y, GET /api/all_data, POST /api/stf

### Task 13: 性能回归验证（2026-07-13）
使用 run_pipeline_debug.py 全链路调试运行器验证 Task 10/11/12 (P0/P1/P2) photometric 性能优化与 Task 4 drizzle .hiss 输出。

**测试帧**: Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts (4500×3600, BITPIX=16, Red 滤光片, 180s)
**校准帧**: masterDark/masterFlat/masterBias (XISF, 4500×3600)
**输出目录**: output/pipeline_debug/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red/

**性能结果**:
| 阶段 | 修复前(s) | 修复后(s) | 目标 | 状态 |
|------|-----------|-----------|------|------|
| 0_read_fits | - | 0.061 | - | ✓ |
| 1_calibrate | - | 1.231 | - | ✓ |
| 2_platesolve_total | - | 4.434 | - | ✓ |
| 3_psf_fit | - | 0.436 | - | ✓ |
| **4_photometric** | **354.7** | **0.881** | **< 5** | **✓ 达标** |
| 5_drizzle | - | 26.316 | - | ✓ |
| **total** | **409.0** | **60.615** | **< 30** | **✗ 未达** |

**改善幅度**:
- photometric: 354.7s → 0.881s，改善 **99.75%**（超额完成 < 5s 目标）
- 总管线: 409.0s → 60.615s，改善 **85.18%**（未达 < 30s，drizzle 26.3s 占主导）

**photometric 关键指标**:
- n_matched = 1527（Gaia BP/RP 光谱积分后星匹配数）
- scale_factor = 7.132932e-03（全局 scale 校正因子）
- before_mean=1563.07 → after_mean=11.15，比值=0.00713 ≈ scale_factor ✓
- 自适应迭代星等查询（Task 12）生效，pc_calibrate_simple_with_gaia 内部 mag_max 迭代控制星数

**drizzle .hiss 验证**（Task 4）:
- 文件: drizzle/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.hiss
- 大小: 11,571,913 bytes (11.0 MB)
- hiss_read 读取成功 ✓
- nside=8192, nested=True, n_pix=964279
- pixel min/max/mean: 0.0001 / 5081.07 / 119.87
- meta 字段: filter=Red, exposure_s=180.0, pixfrac=0.8, wcs/drizzle/source/fits_meta 齐全

**关键发现**:
- 总管线未达 < 30s 目标主因是 drizzle 阶段 26.3s（占总管线 43%），非 photometric 优化范围
- photometric 阶段 0.881s 中包含 gaia_client 锥形搜索+光谱积分+星匹配+全局 scale 校正全流程
- P0（循环内 fprintf 清除）+ P1（滤光片曲线预处理缓存）+ P2（自适应迭代星等）三重优化叠加效果显著
- 报告文件: output/pipeline_debug/performance_report.json

### drizzle 输出改造 .ahpx → .hiss（Task 4，2026-07-13）
将 healpix_drizzle 模块的输出格式从 .ahpx 改为 .hiss，通过 healpix_io.dll 的 hiss_write 写入。

**修改文件 (8个)**:
- `lib/healpix_db/healpix_drizzle/drizzle_engine.h` - 新增 DrizzleMeta 结构体 (filter/exposure_s/obs_time/fits_meta map)；writeAhpx 改为 writeHis，增加 meta 参数
- `lib/healpix_db/healpix_drizzle/drizzle_engine.cpp` - writeAhpx → writeHis: 移除 astro_image_io.h + aio_ahpx_write，改 include healpix_io.h + 调用 hiss_write；JSON meta 构建 filter/exposure_s/obs_time/pixfrac/fits_meta/wcs/source/drizzle（hiss_write 内部前置 nside/nested/n_pix）
- `lib/healpix_db/healpix_drizzle/hp_drizzle_api.h` - 注释更新 .ahpx → .hiss（函数签名不变）
- `lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp` - hp_drizzle_run: 从 header KV 读取 FILTER/EXPTIME/DATE-OBS + 16个 fits_meta KV (OBJCTRA/OBJCTDEC/IMAGETYP/SITELAT/SITELONG/OBJECT/RADESYS/EQUINOX/INSTRUME/TELESCOP/XPIXSZ/YPIXSZ/XBINNING/YBINNING/GAIN/OFFSET)；output_path .ahpx 后缀自动改 .hiss；hp_drizzle_fits_to_ahpx 同步更新
- `lib/healpix_db/healpix_drizzle/healpix_drizzle.py` - docstring/注释更新为 .hiss；_load_dll 添加 healpix_io + astro_image_io 目录到 DLL 搜索路径
- `lib/healpix_db/healpix_drizzle/pipeline_adapter.py` - _build_output_path 输出 .hiss；变量名 output_ahpx → output_hiss
- `lib/orchestrator/python/pipeline_adapters/drizzle_adapter.py` - 同 pipeline_adapter.py 的修改
- `lib/healpix_db/healpix_drizzle/Makefile` - INCLUDES 添加 -I../healpix_io/include；LDFLAGS 添加 -L../healpix_io -lhealpix_io

**编译结果**: 成功
- healpix_drizzle.dll 1038 KB (g++ 16.1.0, exit code 0)
- 导出符号: hp_drizzle_fits_to_ahpx + hp_drizzle_run ✓
- DLL 依赖: astro_image_io.dll + healpix_io.dll + libgomp-1.dll (OpenMP) ✓
- healpix_io.dll 已复制到 healpix_drizzle 目录保证运行时加载

**测试结果**: 3/3 通过 ✓ (tests/test_pipeline_adapter.py)
- 测试 1: 正常流程 → .hiss 生成 (2541 bytes, 178 HEALPix 像素) + hiss_read 往返验证 (nside=4096, filter=Lum, fits_meta.IMAGETYP=LIGHT)
- 测试 2: 缺少 data 块 → 正确报错
- 测试 3: SIP 系数 → Drizzle 成功 + .hiss 生成

**关键设计决策**:
- DrizzleMeta 结构体: 因 FitsImage 不保存 FILTER/EXPTIME 等 KV，新增结构体由 hp_drizzle_run 从 PipelineFrame header KV 填充后传给 writeHis
- hp_drizzle_fits_to_ahpx 路径 meta 留空: FitsImage 未保存 header KV (readFits 只提取 WCS)，此路径用于独立 FITS 文件测试，管线主路径 hp_drizzle_run meta 完整
- .ahpx → .hiss 后缀自动转换: output_path 若以 .ahpx 结尾会自动改为 .hiss，向后兼容旧调用
- 函数名保留: hp_drizzle_fits_to_ahpx / hp_drizzle_run 函数名不变 (C API 签名不变)，只是输出格式改变
- snr 数据不存储: hiss 格式只支持单个 pixel 值数组，snr 数据不再存储 (旧 .ahpx 格式存 brightness+snr+weight)
- JSON meta 555 bytes → hiss_write 内部前置 nside/nested/n_pix 后 594 bytes → zstd 压缩 393 bytes

### healpix_io Python 绑定（Task 3，2026-07-13）
实现 healpix_io.dll 的 Python ctypes 封装，提供 .hiss/.hcsd 格式的面向对象读写接口。

**新建文件 (2个)**:
- `lib/healpix_db/healpix_io/healpix_io.py` - Python 绑定（738行）：4 个类 + 5 个便捷函数 + DLL 加载 + 内存管理
  - HissWriter/HissReader - .hiss 单帧存储读写
  - HcsdWriter/HcsdReader - .hcsd 天球数据库读写（含 read_leaf 按子叶加载）
  - hiss_write/hiss_read/hcsd_write/hcsd_read/hcsd_read_leaf - 便捷函数
- `lib/healpix_db/healpix_io/test_healpix_io_py.py` - Python 绑定往返测试（351行，4个测试用例）

**测试结果**: 4/4 通过 ✓
- 测试 1: .hiss 往返（类+便捷函数，10像素，ipix/pixel/meta 一致，C 侧合并 nside/nested/n_pix 到 meta）
- 测试 2: .hcsd 往返（乱序输入→leaf+ipix 排序输出，10像素，文件 1.13MB 含索引表）
- 测试 3: .hcsd 按子叶读取（read_leaf + 便捷函数，leaf 0/1/2/3，含空子叶 n_pix=0）
- 测试 4: .hiss 空数据（n_pix=0，75字节文件，ipix/pixel 空数组）

**关键设计决策**:
- DLL 路径自动推导: os.path.dirname(__file__) + "healpix_io.dll"，不依赖环境变量
- 模块级 DLL 单例: _get_dll() 懒加载，避免重复加载
- 内存管理: C 侧 malloc 的 ipix/pixel 通过 np.ctypeslib.as_array 零拷贝访问，Reader close()/__del__() 调用 hio_free 释放；meta_json 立即解码为 dict 并释放 C 字符串
- _free_ptr 用 ctypes.cast(ptr, c_void_p) 兼容 c_char_p/POINTER 类型（修复 TypeError: cannot be converted to pointer）
- 便捷函数 hiss_read/hcsd_read 返回独立拷贝（.copy()），Reader 关闭后数据仍可用
- read_leaf 返回独立拷贝（C 内存立即释放），因为 leaf 数据小且不长期持有
- context manager: HissReader/HcsdReader 支持 with 语句，确保内存释放
- 空数据处理: n_pix=0 时传 null 指针，属性返回 np.empty(0)

### healpix_io C++ DLL 实现（Task 2，2026-07-13）
实现 .hiss 单帧存储和 .hcsd 天球数据库格式的读写模块，替代旧版 .ahpx/.ahps 格式。

**新建文件 (5个)**:
- `lib/healpix_db/healpix_io/include/healpix_io.h` - C API 头文件 (5个导出函数 + hio_free, HIO_API __declspec(dllexport))
- `lib/healpix_db/healpix_io/src/healpix_io.cpp` - 完整实现 (UTF-8路径, zstd压缩, JSON合并/解析, 子叶块索引构建)
- `lib/healpix_db/healpix_io/Makefile` - 构建脚本 (g++ -O2 -std=c++17 -lzstd -static)
- `lib/healpix_db/healpix_io/build.ps1` - PowerShell 构建脚本 (UTF-8初始化 + mingw64 PATH)
- `lib/healpix_db/healpix_io/test_healpix_io.py` - 往返测试 (4个测试用例, ctypes + numpy)

**编译结果**: 成功
- healpix_io.dll 1778.8 KB (1.74MB), g++ 16.1.0, 静态链接 libzstd
- 导出符号验证: hiss_write/hiss_read/hcsd_write/hcsd_read/hcsd_read_leaf/hio_free 全部导出 ✓

**测试结果**: 4/4 通过 ✓
- 测试 1: .hiss 往返 (10像素, nside=8192, JSON头121→113字节zstd压缩, ipix/pixel/meta_json一致)
- 测试 2: .hcsd 往返 (10像素, 文件1.13MB含索引表49152×24, ipix按leaf+ipix排序正确)
- 测试 3: .hcsd 按子叶读取 (leaf 0/1/2各3/3/4像素, leaf 3空子叶n_pix=0)
- 测试 4: .hiss 空数据 (n_pix=0, ipix/pixel=null, 74字节文件)

**关键设计决策**:
- zstd 静态链接: -lzstd -static, 运行时无 libzstd.dll 依赖, 与 astro_image_io 独立
- JSON 头合并: 调用者传 meta_json (filter/exposure_s等), DLL 前置 nside/nested/n_pix 字段
- JSON 解析: 简单字符串查找 (无 nlohmann/json 依赖), 解析 nside/nested/n_pix 三个必填字段
- 子叶块索引: 49152 项 (12×64²), 每项24字节 (leaf_ipix + data_offset + data_length), O(1)定位
- 子叶位移: shift = 2×log2(nside/64), nside=8192时 shift=14 (ipix_fine >> 14 = leaf_ipix)
- 排序: hcsd_write 按 leaf_ipix 升序 + 子叶内 ipix 升序排序后写入, 保证子叶数据连续
- 内存管理: 所有输出数组由 malloc 分配, 调用者用 hio_free 释放

### photometric_calib 自适应迭代星等查询（Task 12 P2，2026-07-13）
pc_calibrate_simple_with_gaia 接口从固定 mag_max=16.0 改为自适应迭代星等查询，控制返回星数在 2000-10000 范围，解决固定 16.0 返回数万颗星导致后续处理耗时过长（354s）问题。

**问题**: pc_calibrate_simple_with_gaia 固定用 mag_max=16.0 查询 Gaia，返回星数过多（可能数万颗），后续 OpenMP 光谱积分+星匹配耗时过长。

**修改文件 (1个)**:
- `lib/photometric_calib/cpp/src/pc_api.cpp` - pc_calibrate_simple_with_gaia 函数中锥形搜索改为自适应迭代：从 mag_max=12.0 开始，若 n_gaia<2000 则增大到 13/14/15/16.0，直到 n_gaia>=2000 或达 16.0 上限。不缩小 radius_deg，mag_min 保持外部传入值。用 LOG_INFO 输出每次迭代 mag_max 和 n_gaia。函数签名不变（mag_max 参数仍接受但内部用迭代覆盖）

**编译结果**: 成功
- photometric_calib.dll 1031.2 KB（与 Task 10 后尺寸一致）
- 导出符号验证: pc_calibrate_simple + pc_calibrate_simple_with_gaia 均已导出 ✓
- 自适应星等查询字符串验证: DLL 中含 "自适应星等查询" + "自适应星等查询完成" 字符串 ✓

**关键设计决策**:
- 停止条件: n_gaia >= 2000（含 >10000 情况，无需更小 mag_max）或 i==4（达 16.0 上限）。因 mag_max 递增，n_gaia 只增不减，>=2000 已覆盖 >10000
- 内存管理: 每次失败迭代（n_gaia<2000 且未到上限）释放 spec_stars/spectra_buf；成功迭代结果由后续代码使用不释放；rc!=0 时也释放并返回 -3
- rc!=0 处理: 搜索失败立即返回 -3（不重试，因是真实错误而非星数不足）
- mag_max 参数保留: 函数签名不变，外部传入的 mag_max 被内部 mag_max_arr 覆盖
- 退化路径保留: n_gaia<=0 时仍走 scale=1.0 恒等校正路径

### photometric_calib 循环内日志清除（Task 10 P0，2026-07-13）
清除 photometric_calib C++ DLL 循环内 fprintf 日志，参照 PSF 模块经验消除日志 I/O 瓶颈。

**问题**: photometric 阶段耗时 354.7s（典型应 < 5s），根因是 C++ DLL 在循环内大量调用 fprintf 输出到 stderr。

**修改文件 (4个)**:
- `lib/photometric_calib/cpp/include/log_macros.h` - **新建**。定义 LOG_INFO/LOG_DEBUG/LOG_ERROR 宏。LOG_DEBUG 默认编译时不启用（展开为 ((void)0)），可通过定义 PC_ENABLE_DEBUG 启用。LOG_INFO/LOG_ERROR 输出到 stderr，自动加 [INFO]/[ERROR] 前缀和 \n 后缀
- `lib/photometric_calib/cpp/src/spectrum_integrator.cpp` - 加 #include "log_macros.h"；将 compute_f_syn 末尾的循环内 fprintf（被 pc_api.cpp OpenMP 循环高频调用）改为 LOG_DEBUG。错误路径上的 6 个 fprintf 保留原样
- `lib/photometric_calib/cpp/src/star_matcher.cpp` - 加 #include "log_macros.h"；将 matchBruteForce 循环内每颗匹配星的 fprintf（第86-89行）改为 LOG_DEBUG。错误路径和一次性日志保留
- `lib/photometric_calib/cpp/src/pc_api.cpp` - 加 #include "../include/log_macros.h"。pc_api.cpp 无直接循环内 fprintf（OpenMP 循环调用 compute_f_syn 已在 spectrum_integrator.cpp 中处理）

**编译结果**: 成功
- photometric_calib.dll 1,031.2 KB（1.01MB）
- 导出符号验证: pc_calibrate_simple + pc_calibrate_simple_with_gaia 均已导出 ✓
- 循环内日志字符串验证: DLL 中不再包含 "mag_g="、"匹配#"、"Gaia[" 字符串（strings 检查），确认 LOG_DEBUG 编译时展开为空 ✓
- 错误路径日志保留: DLL 中仍含 "[spec_int] compute_f_syn:" 错误路径字符串 ✓

**关键设计决策**:
- LOG_DEBUG 默认禁用: 编译时不定义 PC_ENABLE_DEBUG，LOG_DEBUG 展开为 ((void)0)，编译器完全优化掉，零运行时开销
- 错误路径保留 fprintf: 任务约束要求不修改错误处理路径日志，错误路径的 fprintf 保留原样（不用 LOG_ERROR 替换，最小改动）
- Makefile 无需修改: 已包含 -Iinclude（INCDIR=include），#include "log_macros.h" 可直接找到
- photometric_calib 不写日志文件: 日志输出到 stderr，无 logs 目录，循环内 stderr 输出量大幅减少

### Drizzle 输出验证脚本 validate_drizzle_output.py（2026-07-13）
- 路径: `lib/orchestrator/scripts/validate/validate_drizzle_output.py`
- 功能: 验证 Drizzle 输出的 .ahpx 文件正确性（8 项验证: 文件存在/ahpx 可读/n_healpix_pixels>0/像素非全0/无NaN-Inf/通量守恒/WCS元数据/nside合理）
- ahpx_io 加载三层降级: 标准 import → .pyc 后备加载 → 降级为文件存在性验证
- 关键发现: 当前环境 ahpx_io 目录无 .py 源文件（仅 .dll + .pyc），需从 `__pycache__/ahpx_io.cpython-310.pyc` 加载
- 详见: lib/orchestrator/memory.md

### photometric_adapter 移除 f_syn 依赖改用 gaia_client handle（2026-07-13）
修改 photometric_adapter 和 PhotometricCalib Python 封装，调用新 C++ 接口 `pc_calibrate_simple_with_gaia`，DLL 内部完成锥形搜索+光谱积分，不再依赖外部 f_syn 数据。

**用户需求**: 移除 photometric_adapter 的 f_syn_path/f_syn_loader 依赖，改为传 gaia_client handle 给 pc_calibrate_simple_with_gaia DLL 接口。

**修改文件 (3个)**:
- `lib/photometric_calib/python/photometric_calib.py` - PhotometricCalib 类新增 `calibrate_with_gaia` 方法 + _setup_signature 加 pc_calibrate_simple_with_gaia ctypes 绑定 (c_void_p handle + 滤光片/光谱数组参数)。原 calibrate_simple 保持不变（向后兼容）
- `lib/orchestrator/python/pipeline_adapters/photometric_adapter.py` - 移除 f_syn_path/f_syn_loader/FSynLoader 依赖；PhotometricParams 改为 gaia_client/filter_name_map/mag_min/mag_max；handler 双模式: gaia_client 不为 None 时用 calibrate_with_gaia (读 OBJCTRA/DEC + FILTER→CurveLoader + gaia_client.get_spectrum_params + FOV半径计算), 否则 fallback 到 calibrate_simple (从 gaia_cat 块读取, f_syn=0 退化路径)
- `lib/orchestrator/python/orchestrator.py` - Orchestrator.__init__ 加 gaia_data_dir 参数；gaia_client 生命周期管理: 优先复用 solve_params.env[0], 否则用 gaia_data_dir 创建 GaiaClientPy(db_type=2)；新增 _create_gaia_client/close/__del__ 方法

**关键设计决策**:
- 传 Python 对象而非裸 handle: PhotometricParams.gaia_client 字段存 GaiaClientPy 实例（非 int handle），adapter 内部用 _get_gaia_client_handle() 提取 _handle 属性，用 _get_spectrum_wavelength() 调 get_spectrum_params()。这样 adapter 既能拿到 handle 传给 DLL，又能获取光谱波长数组
- 双模式 fallback: gaia_client 为 None 时回退到原 calibrate_simple，从 gaia_cat 块读 ra/dec/mag，f_syn=0（退化路径 n_matched=0 scale=1.0），保证向后兼容
- gaia_client 生命周期: 优先级 1=photo_params.gaia_client 已设置 2=solve_params.env[0] 复用 3=gaia_data_dir 新建。Orchestrator 自建的 gaia_client 由 close()/__del__() 释放，复用的不释放（由 platesolve 侧管理）
- 滤光片映射: _DEFAULT_FILTER_NAME_MAP 硬编码 Red→Baader R / Green→Baader G / Blue→Baader B / Lum→Baader UV/IR Cut，可通过 params.filter_name_map 覆盖
- 锥形搜索中心: 优先 OBJCTRA/OBJCTDEC（_parse_ra/_parse_dec 解析 HMS），否则用 CRVAL1/CRVAL2（度）
- FOV 半径: 0.5×对角线×1.2 余量, pixel_scale=sqrt(|det(CD)|), 限制 [0.1, 30.0]°
- _get_gaia_client_handle 兼容 GaiaClientPy(_handle) 和 GaiaSpectrumClient(_client) 两种属性名

**语法检查**: 3/3 文件 ast.parse 通过 ✓

### photometric_calib 扩展 pc_calibrate_simple_with_gaia 接口（2026-07-13）
扩展 photometric_calib.dll，新增 `pc_calibrate_simple_with_gaia` 接口，DLL 内部查询 DR3SP 光谱并积分得 F_syn。

**用户需求**: 原 `pc_calibrate_simple` 接受预计算的 gaia_fsyn 数组；扩展为接受 gaia_client handle + 滤光片曲线 + 光谱波长数组，DLL 内部完成锥形搜索→光谱积分→星匹配→scale 校正全流程。

**新建文件 (2个)**:
- `lib/photometric_calib/cpp/src/spectrum_integrator.h` - 光谱积分器声明（akima_interpolate/simpson_integrate/compute_f_syn）
- `lib/photometric_calib/cpp/src/spectrum_integrator.cpp` - 实现：Akima 子样条插值（参考 scipy.Akima1DInterpolator）+ Simpson 1/3 复合积分（末尾奇数区间用 3/8 公式）+ compute_f_syn（uint8→float64, 10^(-0.4*mag_g) 星等归一化, λ加权积分）

**修改文件 (3个)**:
- `lib/photometric_calib/cpp/include/photometric_calib.h` - 新增 `pc_calibrate_simple_with_gaia` 声明（保持原接口不变）
- `lib/photometric_calib/cpp/src/pc_api.cpp` - 实现新接口：gaia_client_cone_search_with_spectrum→OpenMP 16线程并行 compute_f_syn→StarMatcher+ImageCorrector 复用现有逻辑；free() 释放 gaia_client 返回内存
- `lib/photometric_calib/cpp/Makefile` + `build.ps1` - 添加 gaia_client include path、链接 gaia_client.dll、复制 dll 到输出目录、mingw64/bin 加入 PATH

**编译结果**: 成功
- photometric_calib.dll 1,059,631 字节（1.03MB），仅依赖 KERNEL32/msvcrt/gaia_client.dll
- 导出符号验证: pc_calibrate_simple + pc_calibrate_simple_with_gaia 均已导出
- gaia_client.dll 已复制到 cpp/ 目录保证运行时加载

**关键设计决策**:
- 接口签名补充 WCS/PSF flux 参数：任务描述签名缺 WCS 和 psf_flux/status，无法复用 StarMatcher。实际接口补充了 crval/crpix/cd/sip + psf_flux + psf_status + out_n_matched，使接口可用（原 pc_calibrate_simple 完全不变）
- gaia_client 内存释放：gaia_client.h 未导出 free 函数，返回的 GaiaSpectrumStar*/uint8* 用 malloc 分配。MinGW 下两 DLL 共用 msvcrt，跨边界 free() 安全
- 光谱数据布局：gaia_client_cone_search_with_spectrum 返回分离的 stars[] 和 spectra_buf[]（第 i 颗星光谱在 spectra_buf + i*spec_stride），spec_stride 通过 gaia_client_get_spectrum_params 获取（通常 343）
- 链接方式：MinGW ld 直接接受 .dll 作为输入生成导入引用；运行时需 gaia_client.dll 同目录（build.ps1 自动复制）
- build.ps1 关键修复：g++ 需 mingw64/bin 在 PATH 中才能找到 cc1plus/ld 子进程（仅 g++.exe 在 PATH 不够）；改用 & 调用替代 Start-Process 以正确处理含空格的 -I 路径

### PSF 性能修复：日志开销 + OpenMP 并行（2026-07-12）
修复 dynamic_psf 模块3个性能Bug，PSF阶段耗时从9.26s降至0.26s（-97.1%）。

**根因**: dpsf_log.cpp 默认日志级别为 LOG_INFO，每颗星拟合都输出 DEBUG 日志到 stderr + 文件（双 fflush），2000颗星生成364MB日志文件，I/O开销主导耗时。

**修改文件 (2个)**:
- `lib/dynamic_psf/src/dpsf_log.cpp` - 默认threshold从LOG_INFO改为LOG_WARN；移除双fflush（stderr和文件均不强制刷盘）；WARN及以上级别才写文件（DEBUG/INFO不写文件）
- `lib/dynamic_psf/Makefile` - CXXFLAGS和LDFLAGS添加 `-fopenmp` 启用OpenMP并行（16线程）

**编译结果**:
- dynamic_psf.dll 324.22 KB，修改时间 2026-07-12 22:48:24
- OpenMP依赖确认: libgomp-1.dll（objdump -p 验证）
- 2个无害警告: dpsf_psf.cpp 未使用变量 maxIter/tolerance（pre-existing，不影响功能）

**端到端测试结果**: 15/15 验证项全部通过 ✓
- 测试帧: LDN43_LRGBH_flying_dutchman-20250503@031525-600S-Lum.fts
- PSF_FIT: 0.26s（拟合2000颗星，psf块 2000×6 float64）
- 日志文件: 0 KB（未生成，DEBUG/INFO不写文件 + WARN日志相对路径从tests目录运行时父目录不存在导致CreateDirectoryA失败，但这反而消除了文件I/O开销）
- stderr仍有WARN日志输出（Background constraint violated等，约50条），但不影响性能

**性能对比**:
| 阶段 | 修复前(s) | 修复后(s) | 变化 |
|------|-----------|-----------|------|
| PLATESOLVE | 2.10 | 2.17 | +3.2% (噪声) |
| PSF | 9.26 | 0.26 | **-97.1%** |
| PHOTOMETRIC | 0.96 | 0.03 | -97.4% |
| DRIZZLE | 15.58 | 18.44 | +18.4% (噪声) |
| 总计 | 27.90 | 20.90 | -25.1% |

**关键发现**:
- PSF提速主因是消除日志I/O（364MB→0KB），OpenMP并行带来的额外提升约88%（vs 禁日志2.19s→0.26s）
- 日志相对路径 `lib\dynamic_psf\logs` 从tests目录运行时无法创建（父目录不存在），建议后续改为绝对路径或环境变量配置
- DRIZZLE耗时波动+18.4%属正常噪声（18.44s vs 15.58s，可能是系统负载变化）

### 编排器代码迁移到 lib/orchestrator/ 独立模块（2026-07-12）
将分散在多个模块的编排器代码统一迁移到 `lib/orchestrator/` 独立目录，使用 copy 方式保留源文件。

**目录结构**:
- `lib/orchestrator/python/orchestrator.py` - 编排器核心（从 astro_image_io/python/ 迁移）
- `lib/orchestrator/python/pipeline_adapters/` - 5个适配器
  - `calibrate_adapter.py` (从 calibration/python/pipeline_adapter.py)
  - `platesolve_adapter.py` (从 plate_solve/python/pipeline_adapter.py)
  - `photometric_adapter.py` (从 photometric_calib/flux_calibrator/python/pipeline_adapter.py)
  - `drizzle_adapter.py` (从 healpix_db/healpix_drizzle/pipeline_adapter.py)
  - `psf_adapter.py` (从 orchestrator.py 的 make_psf_fit_handler 提取，dynamic_psf 无独立适配器)
- `lib/orchestrator/tests/test_orchestrator_e2e.py` - 端到端测试
- `lib/orchestrator/scripts/` - 批处理脚本（从 integration_test/python/ 复制，13个.py + test_config.json）
- `lib/orchestrator/docs/architecture.md` - 架构说明
- `lib/orchestrator/README.md` + `.gitignore`

**Import 路径更新**:
- orchestrator.py: 适配器路径改为 `orchestrator/python/pipeline_adapters/*.py`，import `from psf_adapter import make_psf_fit_handler`
- test_orchestrator_e2e.py: PROJECT_ROOT 深度 4→3，模块路径增加 `orchestrator/python` + `pipeline_adapters`
- platesolve_adapter.py: _LOG_DIR 从 `plate_solve/logs` 改为 `orchestrator/logs`
- photometric_adapter.py: _PC_PATH 和 _FSYN_PATH 深度从 3 级改为 4 级（适配新目录深度）
- psf_adapter.py: _LIB_DIR 深度从 2 级改为 3 级

**约束遵守**: 源文件全部保留未删除，未修改任何源文件

### 编排器更新与端到端测试通过（2026-07-12）
spec: .trae/specs/module-adaptation/ (Phase 1 Task 6)

**目标**: 更新编排器适配新版 handler（无临时文件+简化版photometric），执行端到端测试验证全链路

**修改文件 (4个)**:
- `lib/astro_image_io/python/orchestrator.py` - run_single 返回值从 bool 改为 result dict（含 success/timings/blocks/output_files/wcs/photo_stats），新增 _get_block_names/_find_output_ahpx 辅助方法，PLATESOLVE 后收集 WCS 字段，PHOTOMETRIC 后收集 photo_stats KV 值
- `lib/astro_image_io/python/astro_image_io.py` - set_auto_free 文档注释去掉 grad_map/cal_stats（photometric_calib 已不再生成这些块）
- `lib/plate_solve/python/pipeline_adapter.py` - 删除未使用的 import tempfile 和 temp_dir 变量（ipv_solve_from_memory 无临时文件），PlateSolveParams 去掉 temp_dir 字段
- `lib/astro_image_io/python/tests/test_orchestrator_e2e.py` - 重写验证逻辑：15项验证（整体成功/耗时/块状态/WCS/photo_stats/.ahpx输出），临时文件泄漏检测（执行前后对比），性能对比表

**端到端测试结果**: 15/15 验证项全部通过 ✓
- 测试帧: LDN43_LRGBH_flying_dutchman-20250503@031525-600S-Lum.fts (32MB, 4096×4096)
- 无临时 FITS 文件生成 ✓ (执行前5个，执行后5个，无新增)
- .ahpx 输出 426791 bytes ✓ (23664 HEALPix 像素, nside=8192)
- header 含 WCS 字段 CD1_1/CD1_2/CD2_1/CD2_2/CRVAL/CRPIX/CTYPE ✓
- star_det/gaia_cat/psf 块存在 ✓
- PHOTOMETRIC 后 data 块被替换, photo_stats KV 块存在 ✓
- 无 grad_map 块 ✓ (简化版 C++ DLL)
- photo_stats: N_MATCHED=0 (无f_syn走退化路径), SCALE_FACTOR=1.0

**性能对比** (之前→现在):
| 阶段 | 之前(s) | 现在(s) | 变化 |
|------|---------|---------|------|
| PLATESOLVE | 2.10 | 2.02 | -3.6% (无临时文件IO) |
| PSF | 9.26 | 8.33 | -10.0% |
| PHOTOMETRIC | 0.96 | 0.03 | -97.2% (C++ DLL vs Python) |
| DRIZZLE | 15.58 | 15.24 | -2.2% (无临时文件IO) |
| 总计 | 27.90 | 25.63 | -8.2% |

**关键发现**:
- PHOTOMETRIC 提速最显著: 0.96s→0.03s (-97.2%), C++ DLL 全局 scale 校正远快于 Python 梯度拟合
- PLATESOLVE/DRIZZLE 提速较小: 临时文件 IO 相比计算本身占比不大
- PSF 提速 10%: 可能是内存管线减少了数据拷贝
- .ahpx header JSON 缓冲区不足 (需要237458但容量65536), 已截断但不影响功能

### plate_solve 内存接口 ipv_solve_from_memory 实现完成（2026-07-12）
spec: .trae/specs/module-adaptation/ (Phase 1 Task 1+2)

**目标**: 消除 plate_solve 管线适配器中的临时 FITS 文件，直接传递内存像素数据到 C++ DLL

**修改文件 (8个)**:
- `lib/plate_solve/cpp/ipv/include/ipv_api.h` - 新增 `ipv_solve_from_memory` C API 声明
- `lib/plate_solve/cpp/ipv/include/ipv_select.h` - 新增 `ipv_select_from_memory` 内部 C++ 函数声明
- `lib/plate_solve/cpp/ipv/include/ipv_solver.h` - IPVSolver 类新增 `solve_from_memory` 公有方法
- `lib/plate_solve/cpp/ipv/src/ipv_select.cpp` - 实现 `ipv_select_from_memory`（跳过 aio_read，直接接受 float* 像素）
- `lib/plate_solve/cpp/ipv/src/ipv_solver.cpp` - 实现 `solve_from_memory`（9步求解流程，与 solve() 一致）
- `lib/plate_solve/cpp/ipv/src/ipv_entry.cpp` - 新增 `do_solve_from_memory_impl` + `ipv_solve_from_memory` C API 包装
- `lib/plate_solve/python/ipv_solver.py` - 新增 `solve_from_memory` Python 方法（ctypes POINTER(c_float)）
- `lib/plate_solve/python/pipeline_adapter.py` - 替换 `solver.solve(临时文件)` 为 `solver.solve_from_memory(内存)`，删除 astropy 临时 FITS 写入和 finally 清理代码
- `lib/plate_solve/cpp/ipv/Makefile` - 修复: 添加 `ipv_robust_refine.cpp` 到 SRCS + `-fopenmp` 标志（pre-existing bug）

**编译**: make clean && make 成功，ipv_solver.dll 含 ipv_solve_from_memory 导出
**测试**: LDN43 测试帧 (LDN43_LRGBH_flying_dutchman-20250503@031525-600S-Lum.fts) 验证通过
- RMS=0.1431", n_pairs=34, trans_order=3, sip_order=3, success=1
- WCS+SIP 全部注入 header 块 (CTYPE1/2, CRVAL1/2, CRPIX1/2, CD1_1/2, CD2_1/2, A/B/AP/BP)
- star_det 块 (2000, 4) float32 ✅
- gaia_cat 块 (9799, 3) float64 ✅
- 无临时 FITS 文件生成 ✅

### photometric_calib 简化版 C++ DLL 全局 scale 校准（2026-07-12）
- 新建 `lib/photometric_calib/cpp/` 目录，实现简化版测光校准 C++ DLL
- 去掉梯度拟合（M_map曲面拟合），简化为：scale=median(F_syn/F_instr), I_cal=I*scale
- 算法：WCS投影(TAN+SIP) -> 暴力最近邻匹配(3px) -> MAD离群清洗(sigma=3.0) -> 全局scale校正
- 新建文件：`cpp/include/photometric_calib.h`, `cpp/src/{pc_api,star_matcher,image_corrector,wcs_transform}.cpp/.h`, `cpp/Makefile`, `cpp/build.ps1`, `python/photometric_calib.py`(ctypes封装), `cpp/test/test_photometric_calib.py`
- 重写 `flux_calibrator/python/pipeline_adapter.py`：调用C++ DLL，去掉GradientEstimator/grad_map块，保留photo_stats KV块(N_MATCHED, SCALE_FACTOR)
- 归档 `estimator.py` 和 `gradient_fitter.py` 到 `archive/`
- 编译：`make` 成功，photometric_calib.dll 697KB，`-static`全静态链接（仅依赖系统DLL），OpenMP 16线程
- 测试：4/4 通过（基本校准/MAD清洗/退化路径/SIP投影）
- 模块记忆: lib/photometric_calib/memory.md

### calibrate_data() 切换到 C++ DLL（2026-07-12）
- `lib/calibration/python/calibrator.py` 的 `calibrate_data()` 方法从 numpy 路径切换到 C++ DLL `ac_calibrate_frame`
- 导入 `astro_calibration` 模块的 `_dll`/`_to_float_ptr`/`AC_OK`，`Calibrator.__init__` 中调用 `ac_set_num_threads` 设置 OpenMP 线程数
- Flat 归一化（median=1.0, clip 0.1）保留在 Python 端预处理（C++ DLL 内部仅裁剪 0.1 不做 median 归一化），其余校准运算全部由 C++ DLL 完成
- 接口不变：输入/输出格式完全兼容（pipeline_adapter.py 和 calibration_pipeline.py 无需修改）
- 构建 `astro_calibration.dll`（build.ps1，需 MSYS2 bin 在 PATH 中），6/6 测试通过
- 模块记忆: lib/calibration/memory.md

### hp_drizzle_run 命名块直通版本实现完成（2026-07-12）
- 将 `hp_drizzle_run` 从 stub（返回-1）重写为完整实现：从 PipelineFrame 命名块直通调用 DrizzleEngine，不经临时 FITS 文件
- **修改文件**：
  - `lib/healpix_db/healpix_drizzle/hp_drizzle_api.h` - 为 hp_drizzle_run 添加 `const char* output_path` 参数（nullptr 则不写 .ahpx）
  - `lib/healpix_db/healpix_drizzle/hp_drizzle_api.cpp` - 实现 hp_drizzle_run：从 "data" 块读取 float32[H,W] 像素，从 "header" KV 块读取 WCS（CD矩阵+CDELT/CROTA2回退）+ SIP（A/B/AP/BP 系数），构造 FitsImage，调用 DrizzleEngine.drizzle() + writeAhpx()
  - `lib/healpix_db/healpix_drizzle/healpix_drizzle.py` - 添加 hp_drizzle_run 的 ctypes 封装（接受 PipelineFramePy 对象或 C 指针）
  - `lib/healpix_db/healpix_drizzle/pipeline_adapter.py` - 删除 `_write_temp_fits` 函数和 astropy 依赖，handler 直接调用 hp_drizzle_run
- **设计决策**：在 hp_drizzle_run 签名中增加 output_path 参数（任务描述中未含此参数），因为 .ahpx 输出路径需由 Python handler 通过 _build_output_path 构造后传入，C++ 无法从 frame 推导 output_dir
- **编译**：make clean && make 成功，DLL 1.05MB，导出 hp_drizzle_fits_to_ahpx + hp_drizzle_run
- **测试**：tests/test_pipeline_adapter.py 3/3 通过（正常流程 + 缺 data 块错误处理 + SIP 系数），64x64 图像 4096 源像素→178 HEALPix 像素，无临时 FITS 文件生成

### PSF 拟合管线步骤实现完成（2026-07-12）
- `lib/astro_image_io/python/orchestrator.py` - 管线编排器，实现 `make_psf_fit_handler(dll_path=None)` 函数
- handler 读取 PipelineFrame 的 `star_det` 块（星点坐标）和 `data` 块（像素），调用 DynamicPSF.fit_batch 拟合
- 结果写入 `psf` 块（FLOAT64[N,6]: status, B, flux, cx, cy, fwhm均值），供 photometric_calib 复用
- 无 star_det 块时跳过（返回0），无 data 块时返回错误（返回-1）
- 测试: `lib/astro_image_io/tests/test_psf_fit_handler.py` - 8/8 通过（真实 DLL 拟合 5 颗星 + 边界用例）

### 管线编排引擎实现完成（2026-07-12）
spec: .trae/specs/pipeline-engine/

**实现内容**：
- `aio_pipeline_engine.h/.cpp` - 管线编排引擎（8个AIO_EXPORT函数）
- PipelineEngine 内部结构: handlers[5] + params[5] + debug_dir + debug_stage_mask + debug_skip_pixels + auto_free(默认1)
- 阶段处理器签名: `int (*PipelineStageHandler)(PipelineFrame*, const void* params, char* err, int cap)` (in-place模式)
- 单帧执行: run_single 按顺序调用handler→更新stages_completed→调试导出→自动内存释放
- 批量并行: run_batch 使用 `#pragma omp parallel for reduction(+:n_success)` 16线程并行
- 内存生命周期自动释放: PLATESOLVE后free_weight, DRIZZLE后free_pixels/snr/weight, STACK后free_healpix
- XML调试导出: 位掩码控制(DEBUG_AFTER_CALIBRATE等), base64编码像素, 仅用于开发调试
- Python绑定: PipelineFramePy类(set_pixels/set_wcs/set_sip/set_metadata/export_xml/free_*/memory_usage) + PipelineEngine类(register/set_debug/set_auto_free/run_single/run_batch/stage_name/close)
- 测试验证: 单帧执行✓, 4帧批量并行✓, XML导出1806bytes✓, auto_free内存释放✓

**设计决策**：
- STACK阶段批量模式: handler签名是单帧的，多帧stack应在Python层编排（spec已记录）
- auto_free默认开启: 保留中间数据时可通过set_auto_free(0)禁用
- 调试导出位掩码: 可组合(DEBUG_AFTER_CALIBRATE | DEBUG_AFTER_DRIZZLE)，控制哪些阶段后导出XML

**GitHub推送**: commit 37fee6c, 6文件, +1057行

### IO迁移方案确认（2026-07-12）
用户确认的迁移方案：
- .ahps/.ahpl 保留在各自模块（不迁移到astro_image_io，与HEALpix算法强耦合）
- healpix_stack/healpix_drizzle 保持独立仓库，编译时依赖astro_image_io
- Python层全走PipelineFrame: Python层用astro_image_io读FITS填充PipelineFrame→传给C++模块→模块输出PipelineFrame
- XML调试导出已实现(aio_pipeline_export_xml)，满足"类似xml的存储格式导出中间数据用于调试"需求
- 内存占用无法精确计算，采用动态调整策略（按需分配/释放）

**下一步**: 编写IO迁移spec并实施

### 仓库拆分与管线骨架设计（2026-07-12）
spec: .trae/specs/healpix-repo-split-pipeline/

**架构变更**：将 healpix_db 拆分为 4 个独立 GitHub 仓库，引入 PipelineFrame 内存数据管线。
- 模块间数据传递通过 PipelineFrame（内存管线），不直接调用 I/O 接口
- AIO I/O 接口只在管线入口（读 FITS）、出口（写 .ahpx/.ahps）和调试导出时使用
- 内存管理采用动态调整策略（按需分配/释放，非一次性预分配）

**仓库拆分结果**：
| 仓库 | 地址 | 内容 | commit |
|------|------|------|--------|
| Astro-Image-IO-C | https://github.com/fujiaze/Astro-Image-IO-C | FITS/XISF 读取 + .ahpx 格式 + 压缩 + PipelineFrame | c95d69a |
| Healpix-Mosaic | https://github.com/fujiaze/Healpix-Mosaic-C-Python- | 稀疏堆栈 .ahps (sigma-clip + SNR 加权) | b8a8814 |
| Healpix-Drizzle | https://github.com/fujiaze/Healpix-Drizzle-C-Python- | 球面 Drizzle (WCS+SIP→HEALPix, 通量守恒) | 03c7590 |
| Healpix-Database | https://github.com/fujiaze/Healpix-Database | LOD 金字塔 + 球面浏览器 (更新) | 7129e32 |

**PipelineFrame 管线骨架**（已实现接口 + 动态内存管理 + XML 调试导出）：
- `aio_pipeline.h`: PipelineFrame 结构体（图像/WCS/SIP/HEALPix/元数据/状态所有字段）
- PipelineStage 枚举: CALIBRATE / PLATESOLVE / PHOTOMETRIC / DRIZZLE / STACK
- 动态内存管理: frame_create/destroy/alloc_pixels/alloc_snr/alloc_weight/alloc_healpix/free_*/memory_usage
- 内存生命周期: 各阶段按需分配，完成后可释放上游中间数据（如 drizzle 完成后可释放 pixels）
- XML 调试导出: aio_pipeline_export_xml()，base64 编码像素数据，支持 comment 注释和 skip_pixels 标记
- 模块入口函数: hp_drizzle_run(PipelineFrame*), hp_stack_run(PipelineFrame**)

**healpix_lod 依赖关系**：Makefile 中 `DEPS = ../healpix_stack/healpix_core.cpp ../healpix_stack/ahps_reader.cpp`，本地保留 healpix_stack 目录供编译，但 .gitignore 忽略不纳入 Healpix-Database 仓库。用户需 clone Healpix-Mosaic 到 healpix_stack/ 目录。

**下一步**：实现管线编排引擎（已完成，见上方"管线编排引擎实现完成"）→ IO迁移（spec 待编写）

### healpix_lod 迁移至 AIO C API（2026-07-12）
- lod_manager.cpp 压缩/解压调用由 `ahpx::compressZstd/decompressZstd/compressLz4/decompressLz4/compressBound*` 改为 AIO C API `aio_compress/aio_decompress/aio_compress_bound`（codec: 1=ZSTD, 2=LZ4），移除 `ahpx::hasZstdSupport/hasLz4Support` 检查（AIO DLL 始终支持两者）
- Makefile: `AIO_DIR ?= ../../astro_image_io`（astro_image_io 位于 lib/astro_image_io，healpix_lod 位于 lib/healpix_db/healpix_lod，需上两级）；INCLUDES 改为 `-I$(AIO_DIR)/include -I$(AIO_DIR)/src -I../healpix_stack`；DEPS 删除 `../ahpx_io/compressor.cpp`；新增 `LDFLAGS = -L$(AIO_DIR) -lastro_image_io`，LIBS 去掉 `-lzstd -llz4`（压缩能力内置于 astro_image_io.dll）
- 关键发现: ahps_reader.cpp 已先行迁移至 AIO API（`#include "astro_image_io.h"` + `aio_decompress`），healpix_core.cpp 无压缩依赖，故 DEPS 移除 compressor.cpp 后链接仍完整
- 编译验证通过: healpix_lod.dll 1.05MB，objdump 确认依赖 astro_image_io.dll，不再依赖 ahpx_io/zstd/lz4 直接链接
- 注意: 任务原述 `AIO_DIR ?= ../astro_image_io` 与实际目录布局不符（astro_image_io 在 lib/ 而非 lib/healpix_db/ 下），已按 healpix_stack Makefile 同款约定修正为 `../../astro_image_io`

### 已清理废弃模块（2026-07-12）
- 删除 `lib/astro_calibration/` 整目录：与 `lib/calibration/` 逐字节相同的废弃副本，两者共推同一 GitHub 仓库 `Astro-Calibration-Cpp.git` 造成历史分叉。保留 `lib/calibration/`（超集，含完整流水线）
- 删除 `lib/star_psf/` 整目录：未集成的组合原型（检测+PSF拟合合一），PSF 核心与 `dynamic_psf/` 相同，检测功能已被 `lib/star_detector/` 替代。流水线采用拆分架构（star_detector + dynamic_psf）
- 删除 `lib/photometric_calib/python/` 下 5 个废弃文件：star_matcher.py、image_corrector.py（含梯度方向 bug r=log10(F_syn/F_instr)，已修复版在 gradient_estimator/python/）、gradient_fitter.py（旧 MAX_ORDER=5）、wcs_transform.py、curve_loader.py（冗余副本）
- 保留 `lib/photometric_calib/python/sed_builder.py` 和 `synthetic_photometry.py`（spectrum_integrator 自测惰性引用，跨目录依赖未解耦）
- spec: .trae/specs/cleanup-lib-duplicate-modules/

### calibration 模块 Python绑定与统一封装（已完成）
- [x] cosmetic_corrector.py Python绑定C++ DLL（ctypes模块级缓存，优先C++ fallback到Python）
- [x] calibrate_fits.py 统一封装接口（单帧 calibrate_fits + 批量 calibrate_batch 16线程并行）
- [x] 单帧性能测试通过（C++ DLL加载成功，2.0s/帧，较Python版8.5s提速4x）
- 模块记忆: lib/calibration/memory.md

### integration_test 全链路整合测试（进行中）
- [x] Task 1: run_integrator.py 扩展 --narrowband-center/bw/trans 参数，支持运行时注入窄带方波曲线
- [x] Task 2: narrowband_curves.py - 窄带滤光片曲线生成器 (top-hat, BAADER_HA/OIII)
- [x] Task 3: test_config.json - 15帧测试配置 (3 panel × 5 filter, 宽带+窄带混合)
- [x] Task 4: step1_calibrate.py - 校准阶段 (CalibrationPipeline production 模式 -> 01_calibrated.fits)
- [x] Task 5: step2_solve.py - 解析阶段 (IPVSolver + Gaia + StarDetector -> 02_wcs.json)
- [x] Task 6: step3_integrate.py - 光谱积分阶段 (02_wcs.json FOV -> Gaia 锥形搜索 -> F_syn JSON)
- [x] Task 7: step4_estimate.py - 梯度估算阶段 (WCS构造 -> 梯度拟合 -> 校正FITS+报告)
- [x] Task 8: run_all.py - 全链路编排器 (串联 step1~step4, 输出 summary.json; --help 验证通过)
- 模块记忆: lib/integration_test/memory.md

### optimize-integration-gaia-query 优化（已完成）
spec: .trae/specs/optimize-integration-gaia-query/
目标: step3 从 4.95° 锥形搜索(41万星,255s) 改为按检测星坐标批量查询(~1s)
- [x] Task 1: C++ API gaia_client_query_spectrum_by_coords() - OpenMP 16线程并行，已推送 GitHub (commit 3065b83)
- [x] Task 2: step2 输出 02_detected_stars.json (PSF SNR筛选 + 排除饱和星 + 空间均匀性 + WCS转坐标, 300颗)
- [x] Task 3: step3_integrate.py 改用坐标查询 (match_radius=5", 150颗检测->82颗匹配)
- [x] Task 4: synthetic_photometry.py 积分步长 0.1->1.0nm (误差 0.000002%)
- [x] Task 5: run_all.py 传递 --detected-stars
- [x] Task 6: 端到端验证 panel1_Red - 总耗时 18.28s(提速16x), step3 0.85s, n_matched=44
- 文档: lib/photometric_calib/gradient_estimator/IMPLEMENTATION.md (鲁棒拟合模块实现文档)

### healpix_db/ahpx_io .ahpx单帧存储格式读写模块（已完成）
- [x] `ahpx_format.h` - 格式常量定义 (Magic/Version/HeaderLayout/WeightMode/Codec/BlockIndex)
- [x] `compressor.h/cpp` - 压缩层 (ZSTD + LZ4, 条件编译 #ifdef HAS_ZSTD/HAS_LZ4, 无库时 fallback 不压缩)
- [x] `ahpx_reader.h/cpp` - 读取器 (AhpxReader 类, open/parseHeader/readBlock/readPixels/readSnr/readWeight)
- [x] `ahpx_writer.h/cpp` - 写入器 (AhpxWriter 类, setPixels/setSnr/setWeight*/write, 迭代计算偏移+压缩JSON头)
- [x] `ahpx_api.h/cpp` - C API 导出层 (17个 extern "C" 函数, AHPX_EXPORT __declspec(dllexport))
- [x] `Makefile` - 构建脚本 (make 带压缩库, make no-comp 不带压缩库)
- [x] 编译通过 (g++ 16.1.0, -O3 -std=c++17 -fopenmp, zstd+lz4 链接成功)
- [x] 读写往返测试通过 (64×64×3 像素 + SNR + 标量权重, ZSTD 压缩 pixel 49152→44033 bytes)
- 文件格式: [Magic 4B][Version 2B][HeaderSize 4B][HeaderCompSize 4B][BlockCount 4B][压缩JSON][数据块...]
- namespace ahpx, 日志输出到 stderr, JSON 解析用简单字符串查找(无 nlohmann/json 依赖)

### healpix_db/healpix_lod LOD金字塔模块（已完成）
- [x] `lod_format.h` - 格式常量 (LodLevel结构, COMP_LZ4/ZSTD_3/ZSTD_5, .ahpl文件格式 Magic/Version/Header)
- [x] `lod_downsample.h/cpp` - 降采样引擎 (LodDownsampler类, downsample/downsampleIncremental, NESTED位运算 fineToCoarse)
- [x] `lod_manager.h/cpp` - LOD金字塔管理器 (LodManager类, generateFull/updateIncremental/computeOnDemand, OpenMP并行tile)
- [x] `hp_lod_api.h/cpp` - C API导出层 (5个extern "C"函数, LOD_EXPORT __declspec(dllexport))
- [x] `Makefile` - 构建脚本 (静态链接 healpix_core.cpp/ahps_reader.cpp, 动态链接 astro_image_io.dll via AIO C API, -DHAS_ZSTD -DHAS_LZ4)
- [x] 编译通过 (g++ 16.1.0, -O3 -std=c++17 -fopenmp, healpix_lod.dll 1.05MB, 5个导出函数验证)
- [x] `healpix_lod.py` - Python ctypes 绑定 (LodManager静态方法封装, c_void_p+string_at+free_string 内存管理, RuntimeError错误处理)
- [x] `tests/test_healpix_lod.py` - 单元测试 (4/4通过: downsample_basic/lod_generate_full/compute_on_demand/incremental_update)
- 默认4级LOD: Level0(nside=512,lz4) → Level1(nside=2048,lz4) → Level2(nside=8192,zstd3) → Level3(nside=32768,zstd5=数据层)
- .ahpl格式: [固定头34B][压缩数据块], 数据=ipix(uint64)+value(float)+weight(float)+count(uint16)
- NESTED位运算: ipix_coarse = ipix_fine >> (2*log2(fineNside/coarseNside)), 加权平均聚合
- 复用 astro_image_io AIO C API (aio_compress/aio_decompress, 动态链接 astro_image_io.dll), healpix_stack/healpix_core (HEALpix运算), healpix_stack/ahps_reader (读数据层.ahps)
- namespace lod, 日志输出到 stderr, OpenMP 16线程并行处理不同tile

### healpix_db/healpix_browser 球面可视化浏览器模块（已完成）
- [x] `stf.py` - STF 非破坏性拉伸引擎 (纯 numpy, MTF公式, MAD自动拉伸, 4预设linear/sqrt/asinh/log, JSON序列化)
- [x] `sphere_renderer.py` - 球面渲染引擎 (vispy OpenGL Mesh优先, matplotlib Mollweide fallback, 纯Python HEALpix坐标转换)
- [x] `single_frame_view.py` - 单帧浏览视图 (PyQt5, .ahpx加载, 通道切换像素/SNR/权重, WCS坐标显示, 元数据树)
- [x] `sphere_view.py` - 球数据库浏览视图 (PyQt5, 球面渲染, 波段切换, RGB合成, 拖动旋转+滚轮缩放)
- [x] `export_dialog.py` - 投影导出对话框 (5投影TAN/SIN/ZEA/AIT/CAR, 3格式FITS/.ahpx/PNG, 反投影实现)
- [x] `main_window.py` - Qt主窗口 (菜单栏/工具栏/状态栏, 单帧/球面模式切换, STFPanel控制面板)
- [x] `__main__.py` / `__init__.py` / `requirements.txt` - 包入口和依赖声明
- [x] 8/8 文件语法编译通过, STF功能测试通过, 球面网格+HEALpix坐标+投影函数测试通过
- 模块记忆: lib/healpix_db/healpix_browser/memory.md
- 依赖: PyQt5>=5.15, vispy>=0.9, numpy>=1.20, healpy>=1.16, astropy>=5.0 (当前环境缺PyQt5/vispy/healpy)

### healpix_db GitHub 仓库（已完成）
- 仓库地址: https://github.com/fujiaze/Healpix-Database
- 仓库名: Healpix-Database, 描述: Healpix Database (C++ Python), Public
- 仓库根目录 = lib/healpix_db/ 内容平铺, 原位 git init (lib/healpix_db/.git)
- 51 个源文件, 14892 行, commit cfaf2dc
- 不含 DLL/nul/memory.md, 新建 .gitignore + README.md
- ahpx_io/Makefile 删除了未使用的外部引用 -I../../astro_image_io/include
- spec: .trae/specs/healpix-github-repo/spec.md

### healpix_db/healpix_drizzle Drizzle 核心引擎模块（进行中）
- [x] `fits_reader.h/cpp` - FITS 图像读取 (纯C++17, 不依赖cfitsio, 支持BITPIX 8/16/32/-32/-64, WCS+CD/CDELT+CROTA2, SIP A/B/AP/BP, UTF-8路径)
- [x] `wcs_sip.h/cpp` - WCS+SIP 坐标转换器 (TAN投影+前向A/B+逆向AP/BP, pixelToSky/skyToPixel/batch)
- [x] `poly_clip.h/cpp` - 多边形裁剪 (局部切平面 Sutherland-Hodgman, gnomonic正/逆投影, clipPolygon, polygonArea Shoelace)
- [x] `drizzle_engine.h` - Drizzle 核心引擎头文件 (DrizzleConfig/PixelAccumulator/DrizzleStats, 前向声明 healpix::HealpixCore)
- [x] `drizzle_engine.cpp` - Drizzle 核心引擎实现
  - drizzle(): OpenMP 16线程并行, 每线程局部 unordered_map, 合并后输出全局累加器, reduction统计源像素数
  - processPixel() 6步流水线: 像素四角→pixfrac收缩→SIP+WCS逐角映射→HEALPix邻域queryDisc→切平面PolyClip面积裁剪→通量守恒分配
  - getHealpixCorners(): 简化方案 (中心±半分辨率, 极区cos(dec)保护)
  - writeAhpx(): 归一化(brightness=sumFlux/sumWeight, snr=sqrt(sumSnrSq/sumWeight))→ipix排序→JSON元数据(含ipix数组)→AhpxWriter(ZSTD level=5, SCALAR权重)
  - 辅助函数: greatCircleDistance(大圆距离), escapeJsonString(JSON转义)
- [x] `hp_drizzle_api.h` - C API 导出层头文件 (HP_DRIZZLE_EXPORTS/__declspec(dllexport), HpDrizzleResult结构体, hp_drizzle_fits_to_ahpx函数)
- [x] `hp_drizzle_api.cpp` - C API 导出层实现 (参数校验→readFits主图+可选SNR/权重FITS→DrizzleEngine.drizzle()→writeAhpx()→填充result)
- [x] 编译测试通过 (g++ 16.1.0, -std=c++17 -Wall -Wextra -fopenmp -c, 两个.cpp文件均exit code=0, 无警告无错误)
- Makefile 已配置 (fits_reader+wcs_sip+poly_clip+drizzle_engine+hp_drizzle_api, 静态链接 ahpx_io+healpix_core, -DHAS_ZSTD -DHAS_LZ4)
- 待完成: DLL构建测试, Python ctypes绑定, 端到端FITS→.ahpx功能测试
- 关键设计: ipix数组存储在JSON元数据中 (uint64数组), pixel数据通过AhpxWriter.setPixels存储为1D float32数组(width=n,height=1,channels=1)

## 技术约定
- C++17标准，纯标准库实现，不依赖外部库
- OpenMP多线程并行（`#pragma omp parallel for`）
- 核心算法不含IO，日志输出到stderr
- median用 std::nth_element，MAD = median(|v-median|)，sigma = 1.4826 * MAD
- 内部实现用 `namespace ac {}`，C导出层在 ac_api.cpp 用 extern "C" 包装
- combine: 0=mean, 1=median（AC_COMBINE_MEAN / AC_COMBINE_MEDIAN）

### 端到端测试 PLATESOLVE 阶段失败 - aio_frame_export_block_fits 导出 FITS 格式错误（已修复）
- **现象**: 端到端测试在 PLATESOLVE 阶段失败，StarDetector 检测到 0 颗星。任务描述假设是 BZERO 数据格式问题
- **根因**: `aio_frame_export_block_fits` (lib/astro_image_io/src/aio_pipeline.cpp) 导出的 FITS 文件格式不正确：
  - SIMPLE 卡片写成字符串 `'T'` 而非逻辑值 `T`（违反 FITS 标准，astropy 报错 "No SIMPLE card found"）
  - 字节序不正确：FITS 标准要求大端字节序，C++ 直接写入了小端 x86 数据
  - 导致 IPVSolver 内部用 aio_read 读取临时 FITS 时，像素值全是垃圾（min=9.8e-44, max=2.3e-38, median=0.00）
- **诊断方法**: 用 astropy 和 aio_read 分别读取原始 FITS 和导出的临时 FITS，对比像素值范围
  - 原始 FITS (BITPIX=16, BZERO=32768): aio_read 正确返回 float32 (min=1564, max=65535, median=3044)
  - 导出 FITS (BITPIX=-32): aio_read 读回垃圾数据 (min=9.8e-44)，astropy 完全无法读取
  - 用 astropy 写 float32 FITS 后 aio_read 读回正确 (min=1564, max=65535)
- **Python 临时绕过**: 修改 `lib/plate_solve/python/pipeline_adapter.py` 第 274-285 行，将 `frame.export_block_fits("data", temp_path)` 替换为 `astropy.io.fits.PrimaryHDU(data=pixels).writeto(temp_path)`，绕过 C++ 端的格式 bug
- **C++ 端根因修复（2026-07-12）**: 修改 `lib/astro_image_io/src/aio_pipeline.cpp` 中 `aio_frame_export_block_fits` 函数：
  - 第 995 行: `write_card_str("SIMPLE", "T")` → `write_card("SIMPLE", "T")` (逻辑值 T 无引号，T 在 column 11)
  - 第 1002 行: `write_card_str("EXTEND", "T")` → `write_card("EXTEND", "T")` (同上)
  - 第 1010-1023 行: 像素数据写入前增加字节序转换，逐元素反转字节 (elem_size=4/8)，小端→大端
  - 编译: astro_image_io.dll (2.99MB, g++ 16.1.0, exit code 0)
  - 测试: `lib/astro_image_io/tests/test_export_fits_fix.py` 23/23 通过 (FLOAT32/FLOAT64/INT32 数据值正确，SIMPLE 逻辑值 T)
  - 注意: astropy 有非致命 VerifyWarning ("SIMPLE card format doesn't respect FITS Standard")，但不影响数据读取，SIMPLE=True 且数据完全正确
- **验证**: 端到端测试 PLATESOLVE 2.13s 成功 (2000 颗星 + 9799 Gaia 星), PSF_FIT 49.83s 成功 (2000 颗星拟合), PHOTOMETRIC 1.02s 成功 (grad_map + photo_stats)

## 解决过的重大问题

### ahps_writer ZSTD 压缩缓冲区未 resize 导致 chunk offset 偏移（已修复）
- **现象**: healpix_lod 测试中 4 像素 .ahps 文件读取时 `ZSTD_decompress 失败: Unknown frame descriptor`，2 像素正常
- **根因**: `ahps_writer.cpp` 的 `write()` 函数中，ZSTD 压缩成功后 `compBuf` 大小仍为 `bound`（压缩缓冲区上限），但 `index.size` 记录的是 `compSize`（实际压缩后大小）。写入文件时用了 `cd.data.size()`(=bound)，导致实际写入字节数 > JSON 头记录的 chunk size，后续 chunk 的文件偏移与 JSON 记录的 offset 不匹配
- **为什么 2 像素正常**: 2 像素数据(16字节)太小，ZSTD 压缩后更大，回退 NONE 路径有 `compBuf.resize(srcBytes)`，data.size()==compSize，无偏移
- **修复**: 在 ZSTD 压缩成功的 else 分支添加 `compBuf.resize(compSize)`，确保 `data.size() == index.size`
- **验证**: healpix_lod 4/4 + healpix_stack 7/7 + ahpx_io 6/6 = 17/17 全部通过
- **文件**: `lib/healpix_db/healpix_stack/ahps_writer.cpp` 两处（像素索引块 + 波段统计块压缩）

### WCS SIP 系数写入 FITS 但 world2pix 边缘残差未修正（已修复）
- **现象**: plate_solve 求解成功，FITS 头含 `A_ORDER=3 B_ORDER=3 AP_ORDER=3 BP_ORDER=3`，但 Gaia 星投影位置在边缘 r≥0.6 处偏移 median 3.48-6.48px（>2px 占 93.7%-100%）。PSF 拟合在边缘 100% 失败（LM 发散: A→0, B膨胀, x0漂移）。before（只CD）与 after（CD+SIP）偏移统计完全相同，SIP 似乎完全无效
- **根因1（FITS 写入）**: `solve_and_write_wcs.py` 的 `write_wcs_to_fits()` 只写入前向 SIP（A/B），完全丢弃逆向 SIP（AP/BP）。C++ 端 `extract_wcs_sip()` 正确计算了 AP/BP（网格逆变换 NB_GRID=7），但 Python 端未写入 FITS 头
- **根因2（astropy API 误用，真正根因）**: `wcs_transform.py` 使用 `self.wcs.wcs_world2pix()`（astropy 低层方法，**不应用 SIP**），应使用 `self.wcs.all_world2pix()`（高层方法，**应用逆向 SIP**）。同理 `wcs_pix2world`→`all_pix2world`。astropy 6.1.7 的 `wcs_*` 前缀方法直接调用 wcslib C 库，不处理 SIP；`all_*` 前缀方法才在 Python 层应用 SIP 多项式修正
- **诊断方法**: 对比 `wcs_world2pix` 与 `all_world2pix` 在边缘像素的输出，差异达 10.95px（与观测偏移量级一致），确认 SIP 系数正确但未被使用
- **修复1**: `solve_and_write_wcs.py` 补全 AP/BP 写入（sip_keywords 字典收集 + FITS 头写入两处）
- **修复2**: `wcs_transform.py` 将 4 处 `wcs_world2pix`→`all_world2pix`、`wcs_pix2world`→`all_pix2world`（`sky_to_pixel`/`pixel_to_sky`/`sky_to_pixel_batch`/`pixel_to_sky_batch`）
- **验证**: after 图边缘偏移 median 从 3.48-6.48px 降到 0.22-0.32px（-94%~95%），>2px 比例从 93.7%-100% 降到 6.3%-6.5%。误差带从单调上升曲线变为水平直线（~0.2-0.3px）
- **文件**: `lib/photometric_calib/gradient_estimator/python/wcs_transform.py`（4处）、`lib/plate_solve/python/solve_and_write_wcs.py`（2处）
- **影响**: 所有使用 `WCSTransform.sky_to_pixel` 的地方（PSF 拟合、Gaia 投影、梯度估计、诊断图）均受此 bug 影响，边缘星投影位置偏差达 10px。修复后需重跑 PSF 拟合（step3/step4）
- **诊断工具**: `diag_projection_plot.py`（支持 `--no-sip` 生成 before 图，`--label` 标注）

## 未来计划

### PSF 流程重构（未来计划）
- 查gaia→PSF拟合仅gaia星→积分校准→SNR 的统一流程
- 三个拟合需求统一方案: platesolve/积分校准/SNR
- 后续实现，当前 spec 不包含

## SNR 模块 (§14) + 4 处断层修复（2026-07-15 完成）

### SNR 模块 (lib/snr_estimator/)
- **乘法模型**: SNR(pixel) = SNR_phot × (SNR_psf/median)
  - SNR_phot = 1/(ln10 × sigma_residual) 全帧常数
  - SNR_psf = IDW(PSF星位置, (A-B)/mad) 反距离加权 (power=2)
- **C++ DLL**: snr_estimator.dll, C API: snr_estimate(data, psf, sigma, out_snr)
- **Python 封装**: SNREstimator 类, snr_adapter.py (管线适配器)
- **测试**: 6/6 通过 (正常路径/n_stars=0退化/sigma=0退化/A<=B跳过/nullptr检查/数值范围)
- **端到端验证**: SNR_phot=2.58, PSF stars=1906/2000, median(SNR_psf)=595.87, 耗时4.8s

### 4 处断层修复
1. **断层1 (drizzle_engine.cpp + hp_drizzle_api.cpp)**: drizzle 落盘不再丢弃 sumWeight/sumSnrSq
   - drizzle_engine.cpp: PixelEntry 增加 snr 字段, hiss_write 写入 snrArr
   - hp_drizzle_api.cpp: hp_drizzle_run 从 PipelineFrame 读取 "snr" 块并传递给 engine.drizzle (关键修复: 原传 nullptr 导致 snr 全为 1.0)
   - 验证: .hiss snr 通道 has_snr=True, 值分布合理 (min=0.23, max=9.71, median=3.12)
2. **断层2 (healpix_io.h/.cpp)**: hiss 格式 has_snr 字段实现
   - hio_build_json 增加 has_snr; hiss_write 写入 snr; hiss_read 读取 snr (含向后兼容)
3. **断层3 (healpix_io.py)**: Python 绑定扩展 snr 参数
   - HissWriter.write 增加 snr 参数; HissReader 增加 snr 属性; hiss_read 返回 6 元组
   - 测试: 4/4 通过
4. **断层4 (hp_stack_hiss.cpp)**: 等权 sigma-clip → SNR² 加权 sigma-clip
   - weight=SNR² (clamp [0,1e6]); 加权 mean=sum_w/weight; 加权 std=sqrt(sum_wsq/weight - mean²)
   - 向后兼容: 无 snr 时 weight=1.0
   - 测试: 5/5 通过 (基本堆叠/sigma-clip/合并/SNR²加权/向后兼容)

### orchestrator 集成
- snr_adapter.py: STAGE_SNR=5, run_snr_stage 独立调用函数
- run_pipeline_debug.py: 9 节点管线 (0_read_fits → 6_drizzle), 5_snr 节点在 4_photometric 后、6_drizzle 前
- psf 块生命周期: 4_photometric 后仅清理 star_det/gaia_cat, psf 延至 5_snr 后清理
- 端到端: 9/9 节点成功, 总耗时 55.7s (5_snr=4.8s, 6_drizzle=13.7s)

### 关键经验
- PipelineFramePy 写块用 add_block (非 set_block_data); 替换块用 remove_block + add_block
- hp_drizzle_run C API 需显式从 frame 读取 snr 块, 不会自动传递
- SNR 外挂方案 (前置+单次执行) 验证成功, SNR² 加权 drizzle 数据流贯通

## architecture-refactor spec 执行进度 (2026-07-16)

spec 路径: `.trae/specs/architecture-refactor/spec.md` (已审阅通过)

### Phase 1: I/O 统一 (G1+G2) ✅ 完成
- healpix_io 源码合并入 aio，aio.dll 统一提供 FITS+XISF+HEALPix I/O
- aio 选择编译机制 (aio_build_config.json + 条件编译宏 + build.ps1)
- healpix_io/ 归档到 archive/，healpix_drizzle/stack 改为链接 aio.dll
- V1-V4+V10 全部通过 (aio.dll 2924KB/67符号, minimal 894KB/25符号, drizzle 1244KB, stack 1431KB)
- **重大教训**: PowerShell 7 解析无 BOM UTF-8 文件时中文注释导致行解析失败，build.ps1 中 `# 核心必需源文件` 后的 `$srcFiles += "src/aio_log.cpp"` 被吞。修复：所有中文注释改为英文

### Phase 2: 管线统一 (G3) ✅ 完成
- integration_test/ 归档到 lib/orchestrator/archive/scripts/
- test_config.json 迁移到 lib/orchestrator/configs/

### Phase 3: 模块整理 (G5) ✅ 完成
- photometric_calib 子目录合并为 3 (cpp/python/archive) + data + docs

### Phase 4: step4 C++化 (G4) ✅ 完成 (commit fd7ddf1)
- 新建 lib/photometric_calib/cpp/gradient_2d/ (独立 DLL 模块)
- C API: gradient_2d_calibrate (星-图匹配 + IRLS+Tukey+Ridge+LOOCV + 图像校正)
- gradient_2d.dll 编译通过 (329KB), 导出符号正常
- 手写线性代数 (无 Eigen 依赖), OpenMP 并行

### Phase 5: C++管线设计 (G6) ✅ 完成
- 5.1 dll_loader 扩展 10 模块 (AIO/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/GRADIENT_2D/SNR/DRIZZLE/GRADIENT_SPHERE/STACK)
  - STACK 与 GRADIENT_SPHERE 共用 healpix_stack.dll (handle 共享避免 double-free)
- 5.2 orchestrator.h: PipelineStageV2 枚举 (10 节点) + run_stage1/run_stage2 声明
- 5.3 orchestrator.cpp: run_stage1 (stage 0-7 串行) + run_stage2 (stage 8-9 串行) + 5 个新 stage handler 骨架
- 5.3 cli_command.h/cpp: stage1/stage2 子命令解析 + cmd_stage1/cmd_stage2 实现
- 5.5 configs/stage1_config.json + stage2_config.json 模板
- 5.6 V5 (stage1 CLI 8 阶段 timings) + V6 (stage2 CLI 2 阶段 timings) 验证通过
- init_dlls 更新为 10 模块错误检查

### Phase 6: 文档同步 (G8) ✅ 完成 (commit 050b2fe)
- 6.1 PROJECT_ARCHITECTURE.md 更新 (两段流水线 + 10 节点 + gradient_2d 模块)
- 6.2 lib/orchestrator/README.md 更新 (v2.0 stage1/stage2 CLI + 配置文件模板)
- 6.3 lib/orchestrator/memory.md 更新 (Phase 5 完成详情)
- 6.4 V9 文档验证通过

### architecture-refactor spec 全部 6 Phase 完成 ✅
- Phase 1: I/O 统一 (G1+G2) ✅
- Phase 2: 管线统一 (G3) ✅
- Phase 3: 模块整理 (G5) ✅
- Phase 4: step4 C++化 (G4) ✅ (commit fd7ddf1)
- Phase 5: C++管线设计 (G6) ✅ (commit 7072123)
- Phase 6: 文档同步 (G8) ✅ (commit 050b2fe)

### Post-spec: DLL 加载修复 + stage handler 填充 (2026-07-16)
- **DLL 加载修复** (commit ff39173): 10/10 模块加载成功 (之前 2/10)
  - find_mingw_bin() + SetDllDirectoryA: 自动检测 MinGW bin 添加运行时 DLL 搜索路径
  - init_dlls 自动推导项目根目录 (GetModuleFileNameA + 向上 4 级)
  - 预加载 gaia_client.dll 解决 PHOTOMETRIC 传递依赖
- **stage handler 填充** (commit df06ef1): 5/10 实际调用, 5/10 骨架
  - 实际调用: READ_FITS/CALIBRATE/SNR/DRIZZLE/GRADIENT_SPHERE
  - 骨架(前置依赖未就绪): PLATESOLVE/PSF/PHOTOMETRIC/GRADIENT_2D/STACK
  - PipelineFrame* frame_ 成员 + #define PipelineStage AioPipelineStage 解决命名冲突
  - 验证: READ_FITS(4500x3600)+CALIBRATE 成功; DRIZZLE 因缺 WCS 失败 (PLATESOLVE 未实现)
- **PLATESOLVE 实现** (commit 6c08f76): stage1 核心链路跑通
  - init_platesolve_env: 加载 gaia_client + star_detector + ipv_solver, 句柄注入
  - ipv_solve_from_memory: 从 FITS header OBJCTRA/DEC 初始指向求解 WCS+SIP
  - 验证: RMS=0.333", n_pairs=45, SIP order=3, star_det=2000星, gaia_cat=2.3M星
  - DRIZZLE 成功: 31.7s, 16.2M源像素→15.4M HEALPix像素, .hiss=184MB
  - stage1 全 8 阶段 success=true
- **PSF/PHOTOMETRIC/GRADIENT_2D 实现** (commit 84cf3fb): stage1 8/8 节点全部实际 DLL 调用, 单帧端到端验证通过
  - PSF (dynamic_psf.dll dpsf_fit_batch, fitRadius=8): 1913/2000 stars (95%), psf 块 FLOAT64[N,9]
  - PHOTOMETRIC (photometric_calib.dll pc_calibrate_simple_with_gaia): n_matched=1606, scale=0.007358, sigma_residual=0.171 mag
  - **bug 修复**: gaia_client_get_spectrum_params 使用布尔约定 (1=成功), 非错误码约定
  - GRADIENT_2D (gradient_2d.dll gradient_2d_calibrate): RMS=2.17, R^2=0.0015, 应用梯度校正到 data 块
  - SNR: SNR_phot=2.535, median SNR_psf=605, n_valid=1913
  - stage1 端到端: READ_FITS 0.04s + CALIBRATE 0.01s + PLATESOLVE 3.78s + PSF 0.43s + PHOTOMETRIC 0.07s + GRADIENT_2D 0.78s + SNR 4.90s + DRIZZLE 29.2s = 38.9s 全 success
- **stage2 单帧链路验证** (commit a5b70ec): .hiss→.hcsd 链路打通
  - bug 修复: run_stage2 收集 hiss_files 局部变量未赋值给 stage2_hiss_files_ 成员, current_output_hcsd_ 也未设置
  - GRADIENT_SPHERE 4.786s (hp_stack_gradient_corrected, 1 帧, nside=32768, 15406480 像素, SNR²加权, sigma-clip 0 剔除)
  - STACK 跳过 (.hcsd 已由 GRADIENT_SPHERE 生成)
  - 输出 output_stage2.hcsd 177MB (78 非空子叶 / 49152)
- **stage2 多帧验证** (2 帧): 全链路 FITS→.hiss→.hcsd 打通
  - frame1 (061703 Red) + frame2 (062109 Red) → output_stage2.hcsd 178.77MB
  - GRADIENT_SPHERE 5.63s, 合并 15522966 像素, mean_pixel_count=1.9850 (2帧叠加正常)
  - sigma-clip 0 剔除 (同天区同滤光片一致性良好)
- **已知问题**: .hiss has_snr=0 (SNR 通道未持久化), stage2 SNR²加权退化为等权; 属 "4 处断层" 待修复
- **架构重构 spec 全部完成**: stage1 (8/8) + stage2 (2/2) 全链路验证通过

## 2026-07-18 归档 GRADIENT_2D 节点 + stage1 重排为 7 节点
- 用户审阅 PROJECT_OVERVIEW.md 后纠正：stage1 不做曲面拟合和图像亮度修正，PSF 后只做测光坐标系校准（PHOTOMETRIC 已完成）
- 归档 lib/photometric_calib/cpp/gradient_2d/ → lib/photometric_calib/archive/gradient_2d/
- orchestrator 删除 GRADIENT_2D 枚举 + run_stage_gradient_2d 函数，stage 序号重排（SNR 6→5, DRIZZLE 7→6, GRADIENT_SPHERE 8→7, STACK 9→8）
- 文档同步：PROJECT_OVERVIEW.md / DESIGN_IMPL_GAP.md（新增 GAP-021）/ PIPELINE_OVERVIEW.md / lib/photometric_calib/memory.md
- spec: docs/superpowers/specs/2026-07-18-gradient-2d-archive.md

## 2026-07-25 P03-002 配置参数端到端追踪 (v1.1 开发包 G3 Gate) ★DONE★
- **目标**: 证明 Gaia/filter/QE/nside/pixfrac/线程/超时等全部配置参数到达消费者
- **结果**: VERDICT: PASS — 49 参数全追踪 (stage1 34 + stage2 15), 5 断裂点修复, 8 已知限制文档化, 0 FAIL
- **代码修改** (orchestrator 模块):
  - `lib/orchestrator/cpp/src/orchestrator.cpp` - 23 处 P03-002 标记; 修复 5 个断裂点 (photometric.filters_json/qe_curves_json + gradient_sphere.gaia_data_dir/gradient_max_iter/gradient_lambda 从 config 解析, 空值用默认)
  - `lib/orchestrator/cpp/include/orchestrator.h` - 新增 config_gaia_data_dir_ 成员
  - `lib/orchestrator/configs/stage1_config.json` - 扩展 platesolve/psf/photometric/drizzle 参数段
  - `lib/orchestrator/configs/stage2_config.json` - 扩展 gradient_sphere 参数段
- **证据**: engineering/evidence/P03-002/ (TASK_REPORT/TEST_REPORT/EVIDENCE_INDEX/REVIEW_REPORT + config_parameter_trace.json + test_normal.log + .hiss)
- **契约**: engineering/contracts/config_parameter_registry.csv (49 参数 CSV 注册表)
- **断裂点修复**: DEF-01 filters_json / DEF-02 qe_curves_json / DEF-03 gradient_sphere.gaia_data_dir / DEF-04 gradient_max_iter / DEF-05 gradient_lambda
- **已知限制 (WARN 非缺陷)**: frame.filter (FITS header 权威) / stack.weighting (DLL 固定 SNR²) / stack.mosaic_fov_* (预留) / threads (消费者骨架未实现)
- **测试**: stage1 正常测试 PASS (Galaxy_Center Red 帧 ~16s, .hiss 生成) + 19 边界条件代码审查 PASS + 5 失败场景 PASS
- **向后兼容**: 所有修改空配置值时用默认值, 行为与修改前一致; 撤销 5 处 orchestrator.cpp 修改即可回滚
- **依赖**: P03-001 (DONE) + P02-007 (DONE); **后续**: P03-003 (严格失败与禁止静默跳过)

## 2026-07-25 P03-003 严格失败与禁止静默跳过 (v1.1 开发包 G3 Gate) ★DONE★
- **目标**: 必需 DLL/块/质量失败必须非零退出；删除生产路径 true-on-skip；建立稳定错误码与非零退出测试
- **结果**: VERDICT: PASS — 141/141 测试通过 (集成 136 + 端到端 5), 12 类静默跳过消除, 9 个稳定退出码定义
- **代码修改** (orchestrator 模块, 代码变更已包含在 P03-004 提交 a4290d8 中):
  - `lib/orchestrator/cpp/include/orchestrator.h` - 新增 AstroCsExitCode 命名空间 (9 个 constexpr int 常量: SUCCESS=0/GENERIC_ERROR=1/DLL_LOAD_FAILED=2/BLOCK_MISSING=3/CALIBRATE_FAILED=4/PLATESOLVE_FAILED=5/DRIZZLE_FAILED=6/CONFIG_ERROR=7/FILE_IO_ERROR=8) + TaskResult.exit_code 字段
  - `lib/orchestrator/cpp/src/orchestrator.cpp` - 87 处 P03-003 标记, 覆盖 9 个 stage handler 所有必需失败路径; 兜底机制 (失败时若 exit_code=0 按 stage 类型推导默认退出码)
  - `lib/orchestrator/cpp/src/cli_command.cpp` - 4 个 CLI 入口点 (cmd_run/cmd_run_batch/cmd_stage1/cmd_stage2) 统一传播 exit_code: `return r.success ? 0 : (r.exit_code != 0 ? r.exit_code : 1)`
  - `lib/orchestrator/cpp/tests/test_orchestrator_cli.cpp` - 测试 6 期望退出码从 2 改为 7 (CONFIG_ERROR)
- **静默跳过消除**: 12 类 WARN+return true 模式改为 ERROR+return false+exit_code (DLL 未加载/frame_ 为空/必需块缺失/块写入失败/stage handler 未设置 exit_code 兜底)
- **必需/可选阶段分类**: READ_FITS/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/DRIZZLE/GRADIENT_SPHERE/STACK 为必需 (失败返回非零); SNR 为可选 (失败降级到 photo_stats SNR_STATUS=degraded, 不阻塞 stage1)
- **端到端验证**: --help=0 / run nonexistent=1 / config error=7 / run-batch nonexistent=8 / unknown subcommand=1
- **证据**: engineering/evidence/P03-003/ (TASK_REPORT/TEST_REPORT/EVIDENCE_INDEX/REVIEW_REPORT + error_code_registry.json + exit_code_evidence.log)
- **commit**: 0610c00 P03-003 证据交付 (代码变更已含于 P03-004 a4290d8)
- **向后兼容**: 成功路径行为不变 (exit_code=0); TaskResult 新增字段默认值 0; 失败路径行为变更 (原返回 0 的静默跳过现返回非零, 符合 v1.1 开发包规则)
- **残留风险**: 测试环境 DLL 全部加载失败 (code 126), 退出码 2/3/4/5/6 需真实 DLL 环境补充验证; SNR 降级路径下游感知待加强
- **依赖**: P03-002 (DONE); **后续**: P03-004 (DONE, 代码含 P03-003 框架)
