# lib/snr_estimator — 模块记忆（P1-NOISE-DOC）

> 生命周期: P1-NOISE-DOC（本文档重立为追加式模块记忆）→ P1-NOISE-IMPL（落码）→
> P1-NOISE-TEST（可执行测试）→ P1-NOISE-INT。追加式日志，不删改历史段落
> （下方 2026-07-15 / 2026-08-15 为历史登记，其中"乘法模型"一段已被三层
> 科学重构取代——现状以本目录 README.md（r1）与 docs/algorithms/
> NOISE_ESTIMATION.md §13 为准）。

## 2026-09-07 · P1-NOISE-DOC 合同冻结（CONTRACT_READY）

### 任务

- 控制包任务 P1-NOISE-DOC：冻结合同与 README（逐符号源码核对新建，不
  信任旧 README；不声明 IMPLEMENTED，迁移落码由 P1-NOISE-IMPL 执行）。
- matrix 行 P1-NOISE：owner=SA-P1-N17、module_id=astrocs.p1.noise、
  target=astrocs_p1_noise.dll、legacy_paths="lib/snr_estimator;lib/phase1/noise"、
  depends_on_int=P1-CAL-INT;P1-PHOT-INT。

### 产物

- 本目录三件套：README.md（模块合同 10 节，r1）、module.yaml（11 号标准
  §4 manifest，module_status=CONTRACT_READY，entrypoint=MISSING）、
  memory.md（本文件）。
- docs/algorithms/NOISE_ESTIMATION.md §13 增补（ALG-NOISE-001..003 逐符号
  源码锚定 + §13.3 DISP-NOISE-001..009 + §13.4 TEST-NOISE-DESIGN-001
  冻结容差）。
- docs/contracts/DATA_SEMANTICS.md §13（DATA-P1-NOISE）。
- docs/contracts/PUBLIC_API.md API-NOISE-001 节（snr_estimator.h 7 noise
  导出现状 C API）。
- docs/modules/registry/astrocs.phase1.noise-snr.md 事实修订（front-matter
  upstream 更正 + 引言块，占位 ID 收敛声明）。

### 源码核对结论（摘要，行号以实测为准）

- 唯一生产实现 lib/snr_estimator/cpp/src/noise_model.cpp（466 行），签名
  唯一权威 lib/snr_estimator/cpp/include/snr_estimator.h（431 行）；7 个
  noise 导出（头文件行号）：snr_noise_model_v1(:143-149)/_f64(:151-157)/
  _default_config(:115)/_fill(:162-166)/_free(:167-168)/
  snr_noise_scale_law(:173-175)/snr_noise_gain_variance(:178-180)；实现
  行号见下两条。三个时点（HEAD、4b87aef6、工作区）行号一致（git diff
  lib/snr_estimator/cpp/ 为空）。
- noise_model_impl :112-267：参数校验 :118、
  g_model_floor 注册 :126、fixed conservative 掩膜 rmax=max(1,r0)·
  max(1,scale) 默认 10·6=60 px（rmax 定义 :144-145，掩膜段 :128-163，
  不按亮度/振幅缩放，amps 残留未用）、patch 8×8 循环 :179-208、全局兜底
  :207-246、控制点 :241-267；robust_median :42-53/robust_sigma
  (1.482602218505602·MAD) :55-62/collect_patch_sky(5σ≤2 轮) :73-107。
- default_config :333-345：patch 8×8/r0=10/scale=6/clip 5.0/min 64/rounds
  2/spatial 1/floor 1e-12；C ABI 门面 try/catch（v1 :348-356、f64
  :357-365）；fill_impl LS 平面+clamp :371-419（floor<=0 回退 1e-12
  :400-404）；fill 门面 :422-429（双 NULL 拒绝 :425-426）；free :431-444；
  scale_law :447-454；gain_variance :456-464。返回码 0=成功（含
  degenerate 兜底 :238/:266）/1=完全退化（ivar=0，:225-232）/3=参数非法
  或内部异常（malloc 失败 :249-253）。
- 现状单线程顺序（无 omp pragma）；g_model_floor=进程级无锁
  unordered_map 原始指针 key（:32/:126/:400-404/:433）；无取消检查点；
  内存 O(h·w) 掩膜 + O(64) 控制点。
- 生产调用方：orchestrator.cpp:4177（stage6 SNR 必需 stage）→
  :4242-4251 函数指针 snr_noise_model_v1/_f64/_default_config/_fill/
  _free；DLL 装载 dll_loader.cpp:41（snr_estimator.dll）/:55
  （lib/snr_estimator/cpp/）。
- **构建事实（实测，与已冻结文档表述存在出入）**：根 CMakeLists.txt
  **无** snr_estimator 目标（grep rc=1）；snr_estimator.dll 实际由
  lib/snr_estimator/cpp/Makefile:5,12（TARGET、g++ -shared）+
  cpp/build.ps1:29（MinGW 通道）构建；未编入根 CMake 主构建，与
  astrocs_hips/astrocs_drizzle/astrocs_calibration 先例不同——CMake
  集成归 P1-NOISE-IMPL。
- **冻结文档行号锚系统性偏移（实测复核）**：任务书与已冻结文档
  （ALG §13.1 锚表、PUBLIC_API API-NOISE-001、DATA_SEMANTICS §13.1/13.2）
  给出的行号整体错位（如 7 导出头文件 :138-144 实为 :143-149、
  default_config :307-316 实为 :333-345、门面 :318-332 实为 :348-365、
  free :425-438 实为 :431-444；部分锚如 g_model_floor :32、fill floor
  回退 :400-404、SNR_API :7-11 恰好吻合），根源为会话摘要"已锚定"行号
  未经逐条 grep 复核——本 4 文件一律用实测行号；冻结文档勘误随
  P1-NOISE-DOC 收尾由主会话统一处理（父 agent 已批准按实测执行），
  子 agent 不改 docs/**。
- 已冻结 ALG §13.1 与 trace 行 notes 中"编入 snr_estimator CMake 目标"
  为继承任务书口径的误登记——构建事实=Makefile/build.ps1；ALG/trace 行
  修正属文档笔误修订，随 P1-NOISE-DOC 收尾在主会话统一处理，子 agent
  不改 docs/**。
- lib/phase1/noise/noise_model.{h,cpp}（39+67 行）=astrocs::phase1::
  NoiseModel::estimate（median+MAD 单值退化子集，无掩膜/patch/平面场）+
  gain_variance；静态库 astrocs_phase1_noise（CMakeLists.txt:435-438，
  主程序链接 :513）；单测 tests/unit/p1_noise_test.cpp（tests/unit/
  CMakeLists.txt:312-316，6 组）为 P1-005 期旧测，由 P1-NOISE-TEST 对齐
  重锚。
- descriptor 现状：lib/core/src/module_adapters.cpp:489-503
  p1_noise_snr_descriptor，module_id=astrocs.phase1.noise-snr、
  execution_class=cpu_heavy、parallel_ok=true、sci_id=SCI-P1-SNR-001/
  alg_id=ALG-004/data_id=DATA-P1-SNR/test_id=TEST-P1-SNR-001（占位），
  ports: fluxes→DATA-P1-FLUX(ELECTRON/ICRS) 入、snr→DATA-P1-SNR
  (DIMENSIONLESS/ICRS) 出——占位 ID 对齐归 P1-NOISE-INT，不得反向作为
  冻结依据；port DATA 编目（DATA-P1-FLUX/DATA-P1-SNR）为编排层词汇，
  模块合同 DATA 层=DATA-P1-NOISE（DATA_SEMANTICS §13）。

### 纪律记录

- 未改任何生产源码（lib/**/*.c/.h/.cpp 与 CMakeLists.txt 零改动）；
  未改 docs/science/**（SCI-NOISE-001..015 FROZEN，共享引用不改动）；
  禁止据代码缺陷反向修改 SCI——DISP-NOISE-001..009 全部登记
  （NOISE_ESTIMATION §13.3），整改归 P1-NOISE-IMPL/INT。
- 无 git commit/push（子 agent 不提交）；写入一律 LF（Windows CRLF
  仓库，core.autocrlf=false，不动 .gitattributes）。
- 新建/修改仅限 4 文件：lib/snr_estimator/README.md、module.yaml、
  memory.md、docs/modules/registry/astrocs.phase1.noise-snr.md（仅
  front-matter upstream 更正 + `---` 后插引言块，其余正文不动）。

### 待后续任务

- P1-NOISE-IMPL：astrocs_p1_noise.dll、C ABI adapter、plan/execute/
  cancel/inspect、ThreadLease 接线、CMake 主构建集成、版本化 config
  schema、DISP-NOISE-001..009 消化、lib/phase1/noise 旧符号去留登记。
- P1-NOISE-TEST：TEST-P1-NOISE-001 可执行测试（FIX-NOISE-A..G +
  不变量 I1-I6 + 负面矩阵，冻结容差不得放宽：σ5%/场10%/Poisson 5%/
  NumPy rtol 1e-9）；p1_noise_test.cpp 6 组对齐重锚。
- P1-NOISE-INT：descriptor 占位 ID（SCI-P1-SNR-001/ALG-004/DATA-P1-SNR/
  TEST-P1-SNR-001）与 port DATA 编目对齐本合同。
- SCI 层候选（不改 SCI 前提下登记）：min_patch_samples 默认 64 与
  SCI §4 "min_samples 默认 5" 旧稿数字的差异已按"以代码为准"登记
  （ALG §13.2），如需 SCI 侧更新走独立变更。

## 进度日志（历史，保持原样）

### 2026-07-15 v1.0 创建
- 新建 snr_estimator 模块 (spec: snr-module-and-fault-fixes)
- C++ snr_estimate API 实现
- Python ctypes 封装
- 6 项模块自测

### 2026-08-15 V19 三层科学重构 (f672777)
- 新增 noise_model.cpp: PhotometricCalibrationQuality (dex/mag 单位正确化) /
  PsfFitQuality (q_psf=A/residual_scale, legacy (A-B)/mad 退休) /
  NoiseWeightModelV1 (source-masked blank-sky robust variance -> ivar,
  空间平面场, gain/read-noise 交叉验证, 经验 fallback)
- 科学矩阵 noise_model_science_test 32/32 (SNR-001..010/013/014 + 单位/语义)
- legacy snr_extract_model_* 保留为 diagnostic/migration (LEGACY_SNR_SCIENCE_CONSUMER=0)

### 历史职责段（已被三层重构取代，仅存档）
SNR 估算模块,基于乘法模型计算每像素信噪比:SNR = SNR_phot × (SNR_psf/median)。
当前唯一生产科学权重为 NoiseWeightModelV1 blank-sky 稳健方差 → ivar；
旧乘法 snr_estimate*/snr_extract_model* 降级 legacy diagnostic
（snr_estimator.h:19-20 头注释）。
