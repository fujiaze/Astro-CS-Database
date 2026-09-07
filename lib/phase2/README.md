# lib/phase2 — coverage 子模块合同（P2-COV-DOC）

> 本 README 是 **astrocs.p2.coverage 模块合同**（P2-COV-DOC 冻结，
> 2026-09-07，SA-P2-S20；MODULE_MIGRATION_MATRIX.csv P2-COV 行，
> legacy_paths="lib/phase2 coverage sources"，迁移目标
> astrocs_p2_coverage.dll）。lib/phase2/ 同时承载 phase2 其余源
> （sampler/upm/rejection/integrate 等，分别归 P2-SAMP/P2-UPM/P2-REJ/
> P2-INT 各自 DOC 任务，不在本合同域）；原 42 行构建/运行说明全文
> 保留于文末 §13（事实未变，非权威）。落位依据：coverage 生产源
> lib/phase2/src/coverage.cpp（239 行实测）与唯一权威签名头
> lib/phase2/include/astro/phase2/coverage.h（59 行）位于本目录，故
> 合同三件套（README r1 + module.yaml + memory.md）落位本目录。

## 1. 身份（MOD-* ID、DLL target、module/ABI/doc revision、owner、状态）

- MOD ID: **MOD-astrocs-phase2-coverage**；module_id:
  **astrocs.p2.coverage**；dll_name/dll_target:
  **astrocs_p2_coverage.dll**（迁移目标合同值，尚未存在——现状编入
  astrocs_phase2 STATIC，根 CMakeLists.txt:338-346；独立 DLL target
  归 P2-COV-IMPL）。
- module_version 0.11.0-alpha.2（VERSION 实测）；abi_version 1；
  phase_scope phase2；owner SA-P2-S20；状态 **CONTRACT_READY**
  （P2-COV-DOC 冻结 2026-09-07）；entrypoint **MISSING**（registry
  入口未接；descriptor 占位见 §6）。
- doc revision: README r1（P2-COV-DOC 2026-09-07）；源码零改动断言：
  本任务 lib/phase2 生产源 diff=0（git status 白名单核验）。

## 2. 负责范围 / 不负责

**负责**（唯一生产源 coverage.cpp/coverage.h）：

- 输入发现与兼容校验：N 个 Phase1 单帧 HiPS（signal 子目录）逐帧
  校验 hips_order/hips_tile_width=512/hips_version/hips_frame∈
  {equatorial,icrs}/obs_filter 一致性（inspect_frame coverage.cpp:59-140、
  filter 基准比较 :181-193）。
- coverage union MOC：叶级 tile NESTED 父单元聚合
  `t >> 2·(order_f−target_order)` + sort+unique（:204-214），输出
  升序唯一 cell 集（允许不连通分量，coverage.h:9）。
- target_order = min(逐帧 hips_order)（:194-196；冻结语义：禁止低
  order 插值伪装分辨率）。
- 两阶段容量协议（union_cells=NULL 查询 → 分配 → 二次调用回填，
  :218-228）与 POD 清零释放（p2_coverage_free :233-237）。

**不负责**（禁止整 Phase 行为）：

- 不做重校准/PlateSolve/PSF/DR3SP/Drizzle（coverage.h:6 冻结注释）；
- 不读 signal/support/snr 像素数据（只读 properties+Moc.fits）；
- 不做 intersection/depth map/missing-tiles 输出（仅 union，
  DISP-COV-004 如实登记）；
- 不计算任何科学权重（**合同红线：coverage 禁作隐式科学权重**，
  ALG-COV-001 §7；科学权重唯一冻结式 w_UPM=quality_factor×
  geometric_reliability×control_ivar，docs/science/PHASE2_UPM.md §5）；
- 不做控制采样/UPM/排异/积分/HiPS 写出（sampler/upm/rejection/
  integrate/写盘各归其模块合同）；
- 不做坐标系转换（frame 只校验不转换）。

## 3. 输入/输出 ports、DATA ID、单位、坐标、dtype、shape、invalid

唯一权威 = DATA-COV-001（docs/contracts/DATA_SEMANTICS.md §19）。
端口视图（descriptor 词汇，module_adapters.cpp:561-576）：

| 端口 | DATA | 必/可 | 单位 | 坐标 | dtype/shape |
|---|---|---|---|---|---|
| `calibrated`（入， HiPS 树路径） | DATA-COV-001 §19.1 | 必 | 路径（无量纲） | 文件系统 | `const char* const* [n_inputs]` |
| `coverage`（出，union MOC） | DATA-COV-001 §19.2 | 可 | 无量纲整数 | **HEALPix NESTED equatorial/ICRS**（descriptor PIXEL 登记与实际不符，P2-COV-INT 修订） | P2MocCell `[K]`（order/ipix uint64，ipix<12·4^order） |

invalid 语义：路径 NULL/空、hips_order 缺失、tile_width≠512、
hips_version 缺失、hips_frame 非法、filter mismatch → rc=1 显式拒绝
（coverage.cpp:154-157/:88-108/:186-192）；空 filter 静默放行现状 =
DISP-COV-003；K=0（空 MOC）合法 rc=0。

## 4. SCI/ALG/API/ARCH/TEST 链接

- SCI: SCI-P2-COV-001 ⇒ 既有 FROZEN 共享 SCI（SCI-UPM-001
  docs/science/PHASE2_UPM.md / SCI-INT-001 docs/science/INTEGRATION.md /
  SCI-SCOPE-001 docs/science/SCIENCE_SCOPE.md；共享 SCI 不改动，状态
  声明 = docs/algorithms/PHASE2_COVERAGE.md §11.5，P1-WCS-DOC
  SCI-WCS-001=共享 ASTROMETRY.md 先例）。
- ALG: **ALG-COV-001**（docs/algorithms/PHASE2_COVERAGE.md，§2 逐公式
  源码行号锚定 + §11.4 TEST-COV-DESIGN-001 冻结容差 + §11.3
  DISP-COV-001..005）。
- DATA: **DATA-COV-001**（docs/contracts/DATA_SEMANTICS.md §19）。
- API: **API-COV-001**（docs/contracts/PUBLIC_API.md §Coverage union
  C API）+ 编排级 **API-P2-001**（docs/api/PHASE2_API_V1.md，FROZEN，
  所有权图 Coverage 行 / §2 并发五字段行 1）。
- ARCH: **ARCH-001**（cpu_heavy 资源类/单线程 internal_parallel=none/
  host_executor_lease 合同值依据 docs/architecture/CPU_ADAPTIVE_V1.md）。
- MOD: docs/modules/registry/astrocs.phase2.coverage.md（本模块 registry
  合同页）+ docs/modules/phase2.md（legacy 诊断页事实修订）。

## 5. public entry 和实际主要 source symbols（AST/行号实测，非手抄）

- 导出（C ABI，2 个）：`p2_coverage_build`（coverage.h:52 声明 /
  coverage.cpp:144 定义，唯一生产入口）、`p2_coverage_free`
  （coverage.h:56 / :233，POD memset 清零，不释放堆）。
- 内部链接（匿名 namespace）：`parse_props`（coverage.cpp:20-45）、
  `inspect_frame`（:59-140）。
- 结构体：P2MocCell（coverage.h:26-29）、P2HipsInputInfo（:31-38）、
  P2CoverageResult（:40-48）。
- 计划迁移旧符号：无独立旧目录——legacy 即本目录
  lib/phase2/src/coverage.cpp + include/astro/phase2/coverage.h
  （MODULE_MIGRATION_MATRIX.csv P2-COV 行 legacy_paths="lib/phase2
  coverage sources"）；迁移=P2-COV-IMPL 经 C ABI adapter 包装为
  astrocs_p2_coverage.dll，符号集不增减（去留登记其 TASK_RESULT）。

## 6. config schema/default/错误码

- 本模块无独立 config schema：`p2_coverage_build` 无 config 参数
  （coverage.h:52-54），兼容阈值（tile_width=512/frame 白名单）为
  冻结源码常量（coverage.cpp:93/:103）；编排层 stage2.json 的
  coverage 相关键归 P2 session 域（lib/phase2/tools/config_smoke.py）。
- 错误码：rc 0=成功（含 K=0）/1=失败，out->error[512] 载因（7 类
  拒绝路径，DATA-COV-001 §19.1 表）；status 与 rc 同步（"no inputs"
  分支例外=DISP-COV-001）；编排映射 ACS_ERR_PARAM/ACS_ERR_STATE
  （API-P2-001 §4）；default=无参数入口。

## 7. threading/parallel axis/lease/memory/I/O/cancel/checkpoint

- threading_model=host_executor_lease（module.yaml 合同值）；现状
  单单线程、无 OpenMP（coverage.cpp 全文实测 0 处 pragma），
  internal_parallel=none；ThreadLease/取消检查点未接线
  （DISP-COV-005 整改域，同 DISP-WCS-005 先例）。
- 并发合同（API-P2-001 §2 行 1）：reentrant=yes / threadsafe=no
  （独立对象）/ 无内部锁；阶段级取消由编排 session 阶段边界提供
  （p2_session.cpp:120）。
- 内存：全调用方分配（coverage.h:42/:44）；p2_coverage_free 仅 memset
  （:233-237），无堆所有权转移；properties 缓冲 8192 B 栈（:67）。
- I/O：每帧只读 properties + Moc.fits（AIO 唯一 reader，aio_hips_open
  :61；两阶段协议全量重扫 ×2，DISP-COV-005）；单 writer 无输出盘写。
- checkpoint：无（纯函数式扫描，无中间态）。

## 8. provider 能力和 fallback

- 无 provider 轴（cpu_providers=[baseline]，单线程整数集合运算无
  ISA 分支）；AIO reader 为唯一外部依赖（lib/astro_image_io，只读
  冻结模块）；AIO 失败 → error 透传 `aio_hips_reader_last_error()`
  （coverage.cpp:63-65），无 fallback（显式 rc=1）。

## 9. oracle/property/boundary/performance/容差来源

- TEST-COV-DESIGN-001（docs/algorithms/PHASE2_COVERAGE.md §11.4，
  唯一权威）：F1 astropy 独立 union oracle（bitwise 相等，无浮点
  容差）→ F6 确定性/资源；可执行 TEST-P2-COV-001 由 P2-COV-TEST
  落地（EVIDENCE 届时落 EVID-*）。
- 既有 legacy gate：Phase2Coverage.RealHipsUnion（synthetic_gate.cpp:
  3374）/ FilterMismatchRejected（:3410）——Fatduck 本地路径依赖，
  缺失 GTEST_SKIP（:3376/:3413）；合成 fixture 为 P2-COV-TEST 范围。
- 容差来源：§9 集合运算整数精确，容差=0（bitwise），无经验容差。

## 10. build/test 命令、已知限制、未实现项

- 构建：根 CMake `astrocs_phase2` STATIC（CMakeLists.txt:338-346，
  src/coverage.cpp :341）；独立自测 `lib/phase2/CMakeLists.txt:42`
  phase2 STATIC（compatibility 声明，非产品事实源）+
  `phase2_synthetic_gate`（:77，GTest）；无 DLL target（迁移目标
  astrocs_p2_coverage.dll 归 P2-COV-IMPL）。
- 测试：`phase2_synthetic_gate`（18/18 基线，lib/phase2/memory.md）；
  独立 legacy 构建：cmake -S lib/phase2 -B lib/phase2/build && cmake
  --build lib/phase2/build --target phase2_synthetic_gate。
- 已知限制/未实现（不改生产码，DISP-COV-001..005 全清单
  PHASE2_COVERAGE.md §11.3）：status/rc 不一致分支（001）、frame_id
  基名截断（002）、空 filter 放行（003）、intersection/depth/
  missing-tiles 产品缺失（004，matrix 专项四语义仅 union 落地）、
  extern "C" include 卫生+两阶段全量重扫+ThreadLease 未接线（005）。
- git_tracked 自检：本 README/module.yaml/memory.md 以 git add
  stage（不 commit，前台统一验证提交，P1-STAR-DOC 先例）。

## 11. 编排现状（descriptor 占位，如实登记）

- registry descriptor astrocs.phase2.coverage
  （module_adapters.cpp:561-576）：module_id=astrocs.p2.coverage、
  execution_class=cpu_heavy、parallel_ok=true、ports calibrated→
  coverage（DATA-P2-COV/DIMENSIONLESS/PIXEL）、sci_id=SCI-P2-COV-001/
  alg_id=ALG-P2-COV-001/data_id=DATA-P2-COV/api_id=API-P2-001/
  test_id=TEST-P2-COV-001——编排层词汇由 P2-COV-INT 对齐本合同
  （端口坐标 PIXEL→NESTED、coverage 端口产出物=MOC 而非像素图），
  不得反向作为冻结依据。
- 生产调用链：p2_session 阶段 1（p2_session.cpp:119-148，两阶段调用
  :125/:138，manifest 登记 n_union_cells/target_order :145-147）→
  下游 sampler（sampler.cpp:1121/:1138，impl :463）与 stage2 正式入口
  （lib/phase2/tools/stage2.cpp:189-200）；UPM 经控制拓扑间接关联
  （upm.cpp:6-7 注释，不直接消费 P2CoverageResult）。

## 12. 交叉引用（co-located links）

- module.yaml: lib/phase2/module.yaml（CONTRACT_READY）。
- memory: lib/phase2/memory.md（合同冻结追加段见文末）。
- ALG: docs/algorithms/PHASE2_COVERAGE.md；DATA:
  docs/contracts/DATA_SEMANTICS.md#§19；API:
  docs/contracts/PUBLIC_API.md#API-COV-001；编排 API:
  docs/api/PHASE2_API_V1.md；registry:
  docs/modules/registry/astrocs.phase2.coverage.md。

## 13. legacy 构建/运行说明（原 42 行 README 全文保留，非权威）

控制包：`AstroCS_Phase2_Implementation_Control_Package_V1`
（SHA `34A532A2451C8746BEF7B5DA05C3C4C7D15201D66A9D5F6AB5F8F291BE2EB308`）。

构建（MinGW，Fatduck）：

```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cd lib\phase2\build
cmake .. -G Ninja
ninja astrocs-stage2 phase2_synthetic_gate
```

`astrocs-stage2.exe` 依赖 `lib\astro_image_io\astro_image_io.dll`
（运行 PATH 加入）。

运行：

```powershell
astrocs-stage2 <stage2.json>
```

只允许一个 JSON 配置路径参数。示例配置见
`run/phase2/stage2_t4_overlap.json`（真实重叠验证）与
`run/phase2/stage2_full.json`（完整三片）。

合成 Gate：

```powershell
.\phase2_synthetic_gate.exe --gtest_brief=1
```

18/18 PASS（S0/S1/S2/R1/R2/sparse=dense/block/integrate/ACR/robust +
coverage/sampler/upm roundtrip/linear-fit/RCR）。

目录：

- `include/astro/phase2/`：冻结公共接口（upm/coverage/sampler/rejection/
  block/integrate/acr_kernels）
- `src/`：CPU reference 实现
- `tools/stage2.cpp`：正式入口
- `tests/synthetic_gate.cpp`：合成 Gate
