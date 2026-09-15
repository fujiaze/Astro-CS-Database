# W6 · 构建与可移植（第二轮）HEAD a3a343a4→1d2f84fc / 纯静态

> 轴：W6（构建与可移植第二轮：目标可见性与 include 自洽 / 三面对账 / 安装面与导出白名单 §12.3 / C-14 struct_size 扩散 / DLL-SO 符号面 / profile 与 preset / CI 环境依赖 V19-N-05）
> 扫描发起时点 HEAD `a3a343a4`，收口时点 HEAD `1d2f84fc80032486f9a4570fbe6e76e4704266ca`。
> **两时点构建面同 blob**（`git rev-parse` 逐文件比对，同 §九-0），故本文全部数字对两个时点同时成立。
> 权威序：①负责人设计意图 ②现行冻结文本 ③代码/测试自洽
> 纪律：纯静态（read/glob/grep + 本目录内 `python3 -B` 只读解析 + `git --no-optional-locks` 只读子命令）；
> 未执行任何编译器 / cmake / ctest（含 `-N`）/ 测试 / 二进制 / `ci/run.py` / Fatduck 连接 / git 写 / 删除改名；
> 未运行任何无 `--dry-run` 的仓内脚本。静态不能定的，一律标「需运行期，未判」。

## 〇、口径与分母（先锁，后文裸数字均带此口径号）

- **W6-A（构建面文件分母，`git ls-files`）**：`*CMakeLists.txt` **52**；`cmake/*` **7**；`.github/workflows/*` **4**；`ci/*.json` **11**；`include/**` 公共头 **23**。
- **W6-B（CI 门分母）**：`ci/checks.json` → `checks` 数组长度 = **135**（`wc -l` = 4140 行；早先草稿写的 133 系计数脚本取了子集，已订正为 135）。
- **W6-N（CTest 三面账）**：复刻 CI-REG-002 的采集面（活动 CMake 源 = 文件名 `CMakeLists.txt`/`*.cmake`，排除集见下）解析 `add_test(NAME …)` 名集合，再与本轴独立算出的**根图 `add_subdirectory` 可达闭包**（**22/52** 个 CMakeLists 可达，30 个不可达）做交叉。结果：活动面用例名 **211**｜根图可达面 **205**｜不可达子图贡献 **6**｜门 `ctest_targets` 字面 41 + glob 2 → 实际覆盖名 **51**｜根图可达但无任何门 `ctest_targets` 覆盖 **154**（全部由 `ci/ctest_baseline.json` 的 160 条冻结清单兜住）｜`ctest_baseline.json` sources **14** 条。
- **W6-D（环境依赖声明面）**：`ci/checks.json` 中 `prerequisite_tools` 非空的门 = **18/135**；声明值全集 = `{cmake×10, clang×3, llvm-profdata, llvm-cov, pytest, dumpbin, python3:yaml, python3:astropy×2, python3:numpy×6}`。
- **W6-E（未声明工具字面量）**：判据 = 门的入口脚本 + 其一级 import 内出现的 `argv[0]` 工具名字面量，与 W6-D 声明集比对 ⇒ **74** 道门存在未声明外部工具调用。此口径比 V19-N-05 的 AST-`subprocess`-argv 口径宽，两者不可混用（V19 为 21 道）。
- **W6-H（struct_size 普查）**：受跟踪 `.h/.hpp` **203** 个，排除 `*third_party*`、`lib/acr/**`、`healpix_browser_qt`、`orchestrator`、`*/archive/*`；只计匿名形 `typedef struct { … } X;` ⇒ **124** 处：带 `struct_size` **20**｜仅带 `acs_head` **23**｜两者皆无 **81**（其中 70 处位于 `*/include/` 公共面）。
- **W6-J（跨语言镜像）**：Python `ctypes.Structure` 镜像类中本轴逐份数出字段的 **11** 份（aio 8 + gaia 3），另 ipv 家族不计入（已修方）。
- **W6-Q2（头自洽）**：公共头 **69** 个（`include/` + 13 个模块 `include/` + `providers/cpu/*/include/` + `modules/services/io/include/` + `runtime/{module_loader,registry}` + `lib/gaia_xpsd_client/src`）；引号 include 按"同目录 + 上述根集"解析；符号声明闭包 = 引号/系统 include 的传递闭包。
- **W6-R（构建树生产/消费）**：以门命令 + `ci/steps/*` + `CMakePresets.json` 字面量对账目录名的生产者与消费者。
- **全量排除前缀（凡"全仓 grep"处均适用，写成 ./x）**：`./问题扫描` `./工程控制` `./设计大纲` `./evidence` `./reports` `./docs/archive` `./AstroCS.wiki` `./engineering/control` `./run` `./build` `./third_party` `./GaiaDR3` `./GaiaDR3SP` `./lib/astro_image_io/third_party` `./lib/orchestrator/cpp/third_party`。`run/**` 影子树里有 `CMakeLists.txt`/`ci/checks.json` 的旧副本，任何 naive `grep -r` 会被污染，本轴一律先过 `git ls-files`。
- 本轴条目号：**W6-N-01 … W6-N-09**（宁窄而实；上限 15，本轴发 9 条）。

## 一、三面清单（CMakeLists ↔ checks.json ↔ workflows）

账（口径 W6-A/W6-B/W6-N/W6-R，时点见页眉）：

| 面 | 实测 |
|---|---|
| CMakeLists 总数 / 根图 `add_subdirectory` 可达 | 52 / **22**（不可达 30：`cli/` + ACR 全树 25 + `lib/healpix_db/healpix_browser_qt` + `modules/conformance/echo` + 2 个 vendored third_party） |
| `add_test(NAME)` 名（活动面） | 211（根图可达面 205） |
| 门 `ctest_targets` | 字面 41 + glob 2 = 覆盖 51 名 |
| `ci/ctest_baseline.json` | targets 160｜sources 14｜0 名在源面找不到（C5 不红） |
| CI 门 | 135 道｜`prerequisite_tools` 非空 18｜profile×platform 组合 8 档（63/54/6/5/4/1/1/1） |
| workflows | `ci-linux.yml`/`ci-windows.yml`（+2 非 CI）｜与注册表对账由 `WORKFLOW-REGISTRY-BINDING`/`ci/validate_workflow_binding.py` B1–B16 承担（本轴复算未发现新违例，见 §十） |

三面里唯一本轴新出的账：**"已登记"与"可达/可生成"是两个不同集合，而门只对前一个集合负责**（→ W6-N-01）。
另外 154/205 用例的"注册"完全依赖一次性冻结基线（`ctest_targets` 显式注册覆盖率 51/205 ≈ 25%），这是账面事实，不是缺陷本身（C2(b) 是设计选择），故不单列条目。

## 二、目标可见性与 include 自洽（头能否独立编译）

见 W6-N-07（正题）与 §十-1/§十-2（两条查了不成立的，含假阳性剔除记录）。
关键事实：根 `CMakeLists.txt` **没有任何全局 `include_directories()`**（`grep -n '^[a-zA-Z ]*include_directories(' CMakeLists.txt cmake/*.cmake` → 0 命中），各 target 自带 PUBLIC 面 ⇒ "以仓库根为 -I" 只存在于 `tests/unit/CMakeLists.txt`、`tests/abi/*` 的手工编译里；`runtime/registry/module_registry.h:6` 的引号 include 因此在根图内不可解析。

## 三、安装面与导出白名单（§12.3 / BLD-003）

见 W6-N-03（安装面闭合性 + 校验器零注册）与 W6-N-05（导出符号面）。
账：`cmake/install_layout.cmake`（140 行）自称"唯一 install 规则源 + clean install 仅白名单（机器校验 `packaging/verify_install_tree.py`）"；`packaging/install-tree.contract.json` units **16**；`packaging/astrocs.product.json` units **10**（差集 = 3 许可证 + 2 schema + manifest 自身，属设计内；**路径逐条比对 0 不一致**，见 §十-4）；`packaging/**` 在 changed_paths 里只命中 2 道 windows-main 门。

## 四、跨语言结构 struct_size（C-14 扩散到 aio/gaia/dynamic_psf 复验）

见 W6-N-04。结论：**三家未做，V11 四站在收口时点全部仍未修**，且镜像脚本仍零采集面。

## 五、DLL/SO 符号面

见 W6-N-05。正对照（写清楚，避免被读成"符号面全坏"）：`lib/{drizzle,calibration,cosmetic,hips}/src/module_exports.map` 四份 `global:` 段逐字只列 `astrocs_module_query_v1;`，对应 `astrocs_p1_*.def` 四份 `EXPORTS` 段亦只列同名符号，8 文件彼此自洽、与 `12_DLL_ABI_AND_LOADER_STANDARD` §6 一致。

## 六、profile 与 preset 一致性

见 W6-N-06。

## 七、CI 环境依赖声明（V19-N-05 复验 + 新站）

见 W6-N-02。

## 八、条目总表

| 编号 | 优先级 | 一句话 | 证据锚 | related |
|---|---|---|---|---|
| W6-N-01 | P1 | CTest 登记面把 6 个"结构上永不可生成"的用例名当存量在册（browser_qt×5 + conformance/echo×1），门无"可达性"维度 | W6-N；`ci/ctest_baseline.json:93`；`lib/healpix_db/healpix_browser_qt/CMakeLists.txt`；`modules/conformance/echo/CMakeLists.txt`；`tools/quality/check_ctest_registration.py` C1/C2(b)/C5 | M8-F-005、V19-N-06、簇1 机制① |
| W6-N-02 | P1 | V19-N-05 未修：18/135 门有声明，值集合仍无 g++/gcc/nm/objdump/tar/bash/taskset/ctest；新站合计 74 门未声明；`validate_registry` 仍只有 R10/R11（V19 建议的 R12 未落地） | W6-D/W6-E 实测；`ci/validate_registry.py`；`tools/check_legacy_exit.py:21`；`tests/backend/test_isa_*.py`；`tools/make_linux_release.py`；`lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_taskset_invariance.sh` | V19-N-05、V19-N-06、簇1 机制⑤ |
| W6-N-03 | P1 | 安装白名单不闭合：`install(DIRECTORY packaging/schemas/ PATTERN "*.json")` 实装 4 文件 vs 合同只登记 2 个 SCHEMA unit；唯一校验器 `verify_install_tree.py` 三面零注册且无"白名单外多余文件"判据 ⇒ Linux 安装面 0 门 | `cmake/install_layout.cmake`（schemas install 段）；`git ls-files packaging/schemas/*`；`packaging/install-tree.contract.json`；`packaging/verify_install_tree.py`；checks.json 解 0 命中 | M5b-G-05、M5b-G-04、M8-F-003/L23-003、V14-N-05 |
| W6-N-04 | P1 | C-14 三家扩散未做：aio/gaia/dynamic_psf/star_detector 边界结构 0 `struct_size`（口径 W6-H 124/20/23/81），Python 11 份镜像 0 `struct_size`，V11-N-01/02/03/04 四站在收口时点全部未修，镜像脚本仍零采集 | `lib/astro_image_io/include/aio_hips.h:69/:84`；`lib/gaia_xpsd_client/src/gaia_client.h:57`；`lib/dynamic_psf/include/dynamic_psf.h:17/:38`；`lib/plate_solve/cpp/ipv/include/ipv_api.h:78`（有）vs `:39`（无）；`lib/astro_image_io/tests/hips_direct_smoke.py:24/:34`；`lib/plate_solve/tools/diag_gaia_psf_projection.py:78/:210-218`；`…/gate2_psf_oracle.py:158-166` | C-14、V11-N-01/02/03/04/06/07/09/10、机制⑪、V2-N-01（唯一修成的一家） |
| W6-N-05 | P1 | 导出面合同无机器消费者：`module_dll_contract.schema.json` 全仓 0 个 `.py`/CMake 消费者（只有文档提它），Windows 唯一符号门判据是"dumpbin 输出非空"，Linux 侧无任何采集，而 3 个平台 target 开着 `WINDOWS_EXPORT_ALL_SYMBOLS ON` | `contracts/config/module_dll_contract.schema.json`；`tools/quality/ci_windows_driver.py:606+`；`CMakeLists.txt:131/:137/:151/:228`；`git grep -ln module_dll_contract` 结果（仅 README/REVIEW/docs、noop README） | M5b-G-14、V11-N-09、V19-N-05、§12.3-6 |
| W6-N-06 | P2 | preset 契约悬空 + 校验器零注册：`CMakePresets.json:14` 的 `contract_schema` 指向不存在的 `packaging/schemas/preset-contract.schema.json`（真源 `preset-contract.json`）；声明的校验器 `cmake/toolchain/verify_toolchain.py` 在 135 门里 0 引用；`CMakePresets.json` 的 changed_paths 只被 3 道 windows-main 门覆盖 | `CMakePresets.json:3,14`；`cmake/toolchain/verify_toolchain.py:7/:46`；`DEPENDENCIES.md:6/:26`；`packaging/schemas/` ls；`build.sh`（不读 preset，`cmake -S lib/phase2`） | M5b-G-01、§12.3-9、簇1 机制⑥ |
| W6-N-07 | P2 | 头自洽实测缺口：`runtime/registry/module_registry.h:6` 引 `"runtime/module_loader/secure_loader.h"` 在根图任何 -I 下不可解析（头自称"独立可编译"）；另 8 个公共头用 `size_t` 而闭包内无 stddef/cstddef | W6-Q2；`runtime/registry/module_registry.h:4-6`；`include/astrocs/core/{artifact_store,checkpoint,logging,module,trace}.h`、`include/astrocs/io/io_adapter.h`、`lib/plate_solve/cpp/ipv/include/{ipv_kvector,ipv_types}.h`；仓内无 `-fsyntax-only`/自包含头门（`git grep fsyntax-only` 命中仅文档） | 需运行期（能否编译）未判；簇1 机制⑥ |
| W6-N-08 | P2 | ACR-DORMANT 的符号面判据锚在 CI 三面零生产者的目录且是同族 4 工具里唯一没做候选链的：`build/root-cmake/astrocs` 不存在 ⇒ `nm` 段整体静默跳过（连 SKIP 都不打印） | `tools/check_legacy_exit.py:19/:67/:20`；正对照 `tools/check_duplication.py:24`、`tools/check_reproducible_build.py:31`、`tools/check_warning_suppression.py:62/:100`；`ci/steps/linux_build_root_graph.sh`（实产 `build/`、`build/cli`、`build/linux-openmp-on`、`build/linux-control`）；`tools/check_link_scan.py:9` 同锚 | M5a-G-008、M5b-G-10、簇1 机制①③ |
| W6-N-09 | P2 | `p1_noise_adapter` 门卫的"自动复活"承诺在受跟踪面缺落点：`if(TARGET p1noise_under_test AND TARGET astrocs_p1_noise)` 的第二个条件 `add_library(astrocs_p1_noise` 全仓 0 命中，且 `install_layout` 的 foreach 与该 target 无关 ⇒ 该用例永久零注册，却在 baseline + known_failures(conditional) 双在册 | `tests/unit/CMakeLists.txt:1159-1166`；`lib/snr_estimator/tests/p1noise/CMakeLists.txt`（只定义 `p1noise_under_test`/`p5snr_under_test`）；`git grep -n 'add_library(astrocs_p1_noise'` → 0；`ci/known_failures.json:46-54`；`cmake/install_layout.cmake:100` 附近注释 | V14-N-05、F-CI-002-01、install_layout 注释、簇1 机制② |

## 九、三级复核记录（判"缺失"必贴命令与输出）

**0）两时点构建面同 blob（防"扫的是旧树"）**

```
$ git --no-optional-locks rev-parse HEAD
1d2f84fc80032486f9a4570fbe6e76e4704266ca
$ git --no-optional-locks diff --stat a3a343a4 HEAD -- ci/checks.json ci/validate_registry.py ci/run.py \
    CMakeLists.txt cmake packaging CMakePresets.json .github \
    lib/astro_image_io/tests lib/plate_solve/tools lib/photometric_calib/cpp/test
（空输出 = 两时点这些路径无差异）
$ for p in CMakeLists.txt cmake/install_layout.cmake CMakePresets.json packaging/install-tree.contract.json ci/validate_registry.py; do \
    echo "$p $(git rev-parse a3a343a4:$p) $(git rev-parse HEAD:$p)"; done
CMakeLists.txt                 cd2eb82b… cd2eb82b…
cmake/install_layout.cmake     a5757740… a5757740…
CMakePresets.json              c7f5aec9… c7f5aec9…
packaging/install-tree.contract.json 80fb5e44… 80fb5e44…
ci/validate_registry.py        48e7eee0… 48e7eee0…
```

**1）判"6 个用例名不可达但在册"（W6-N-01）**

```
$ python3 -B 问题扫描/_verify/_w6_conditional_tests.py
口径 W6-N：CI-REG-002 视角的活动 CMake 源 = 52 | 根图 add_subdirectory 可达闭包 = 22
CI-REG-002 采集面目标名: 211  根图可达采集面目标名: 205
在采集面但不在根图（不可达子图）: 6
   OFF-GRAPH-TARGET browser_backend        ['lib/healpix_db/healpix_browser_qt/CMakeLists.txt']
   OFF-GRAPH-TARGET geometry_truth         ['lib/healpix_db/healpix_browser_qt/CMakeLists.txt']
   OFF-GRAPH-TARGET healpix_math           ['lib/healpix_db/healpix_browser_qt/CMakeLists.txt']
   OFF-GRAPH-TARGET hips_browser_backend   ['lib/healpix_db/healpix_browser_qt/CMakeLists.txt']
   OFF-GRAPH-TARGET stf_engine             ['lib/healpix_db/healpix_browser_qt/CMakeLists.txt']
   OFF-GRAPH-TARGET module:astrocs.conformance.echo ['modules/conformance/echo/CMakeLists.txt']
baseline 中属不可达子图: ['browser_backend','geometry_truth','healpix_math','hips_browser_backend','module:astrocs.conformance.echo','stf_engine']
```
二级复核（这两个子图是否有别的构建入口）：
```
$ git --no-optional-locks grep -ln healpix_browser_qt -- *CMakeLists.txt *.cmake *.yml *.json *.sh *.py
→ ci/ctest_baseline.json, 该子图自身 CMakeLists, 其 tools/*, tests/arch/test_single_cli.py,
  tests/cli/test_cli_single_install.py, tools/{assemble_v17_review_pkg,gen_repo_source_manifest}.py,
  tools/quality/{build_v19r4_package,update_audit_status,v19r3_audit,v19r3_static}.py   ← 全部非 add_subdirectory
$ git --no-optional-locks grep -ln conformance/echo -- *CMakeLists.txt *.cmake *.yml *.json *.sh *.py
→ ci/ctest_baseline.json, modules/conformance/echo/CMakeLists.txt, tests/abi/test_abi005_echo.py
```
三级复核（读门判据文本确认无"可达性"维度）：`tools/quality/check_ctest_registration.py` docstring C1/C2/C5 —— C1 采集 = "名为 CMakeLists.txt 或 *.cmake 的文件"，C2 = "门 glob 或基线命中"，C5 = "基线名是否还在源里"；无任何 `add_subdirectory`/可达性概念。

**2）判"V19-N-05 未修"（W6-N-02）**

```
$ python3 -B 问题扫描/_verify/_w6_final_numbers.py | sed -n '/W6-D/,$p'
口径 W6-D 声明 prerequisite_tools 的门: 18 / 135
   声明值集合: {'cmake': 10, 'clang': 3, 'llvm-profdata': 1, 'llvm-cov': 1, 'pytest': 1,
                'dumpbin': 1, 'python3:yaml': 1, 'python3:astropy': 2, 'python3:numpy': 6}
   g++ 被声明次数 0 / gcc 0 / nm 0 / objdump 0 / tar 0 / bash 0 / taskset 0 / ctest 0 / make 0
```
站点定位（逐条 `git grep`）：
```
nm          → tools/check_legacy_exit.py:21,69 ; tools/check_duplication.py:30 ; tools/quality/check_prod_reachability.py:111
objdump     → tools/quality/check_isa_leak.py:49 ; tools/check_baseline_opcodes.py:18 ; tests/backend/test_isa_avx*.py(5) ; tests/cpu/avx512/check_avx512_illegal_instr.py
tar         → tools/make_linux_release.py ; tests/quality/test_linux_release.py
taskset     → lib/healpix_db/healpix_drizzle/tests/p1drz/p1drz_taskset_invariance.sh ; …/p1drz_merge_pipeline_lock.sh ; tests/backend/test_hardware_inspect.py
python 裸 import（口径 W6-E 口径下的姊妹门反差）：
   UT-BACKEND declared=[]  实测 tests/backend 裸 import 文件数 {'numpy':5,'astropy':4}
   UT-IO     declared=[]  实测 tests/io  {'numpy':4,'astropy':4}
   UT-CLI    declared=[]  实测 tests/cli {'astropy':1}
```
`validate_registry` 规则面：`grep -n 'R1[0-9]\|def check_R' ci/validate_registry.py` → 只有 R10/R11 两族，V19-N-05 建议的 R12（声明与实现比对）无实现。

**3）判"安装白名单校验器零注册 / 白名单不闭合"（W6-N-03）**

```
$ python3 -B 问题扫描/_verify/_w6_install_gates.py
   引用 verify_install_tree 的门: []            ← 135 道门 command/args 全量解析
   packaging/** 出现在 changed_paths 的门: ['WIN-PACKAGE-CANDIDATE','WIN-CANDIDATE-VALIDATE']（均 windows-main）
$ git ls-files packaging/schemas/*
packaging/schemas/astrocs-product.schema.json  packaging/schemas/dependency-lock.schema.json
packaging/schemas/install-tree-contract.schema.json  packaging/schemas/preset-contract.json
$ python3 -B -c "…install-tree.contract.json…"
   contract 中 schemas/ 条目: ['schemas/install-tree-contract.schema.json','schemas/astrocs-product.schema.json']
```
`cmake/install_layout.cmake` 的 `install(DIRECTORY packaging/schemas/ DESTINATION schemas FILES_MATCHING PATTERN "*.json" PATTERN "*.schema.json")` ⇒ 4 个文件入树，其中 `dependency-lock.schema.json`、`preset-contract.json` 无 unit；licenses 面 3 文件 = 3 unit（一致，作为对照）。

**4）判"preset 契约文件不存在 / 校验器零注册"（W6-N-06）**

```
$ python3 -B 问题扫描/_verify/_w6_batch3.py
   CMakePresets.json:14:      "contract_schema": "packaging/schemas/preset-contract.schema.json",
   cmake/toolchain/verify_toolchain.py:46:CONTRACT_RELPATH = "packaging/schemas/preset-contract.json"
   实际存在: True False        ← preset-contract.json 在；preset-contract.schema.json 不存在
$ python3 -B 问题扫描/_verify/_w6_batch5.py（片段）
   引用门: []                  ← cmake/toolchain/verify_toolchain.py 无任何门引用
   changed_paths 含 CMakePresets.json 的门: ['WIN-BUILD-RELEASE','WIN-TEST-UNIT','WIN-PACKAGE-CANDIDATE']
```

**5）判"struct_size 未扩散 / 镜像未修"（W6-N-04）**

```
$ python3 -B 问题扫描/_verify/_w6_v11b.py
   hips_direct_smoke.py:24 AstroSphereTileView n=7 ['parent_ipix','leaf_order','width','data_type','flux_sum','covered_area','valid_mask']
   hips_direct_smoke.py:34 AioHipsSnrPoint      n=4 ['ra_deg','dec_deg','snr','source_id']        ← 仍含废止名 source_id
   hips_mapping_oracle.py:29 / v5_maptile_oracle.py:29 / v5_snr_precision_roundtrip.py:23 / gen_hips_browser_test.py:31  n=7（五份全缺 var_num_sum）
   v5_snr_precision_roundtrip.py:33 AioHipsSnrPoint n=6 ; gen_hips_browser_test.py:41 n=6
   diag_gaia_psf_projection.py:78 GaiaSpectrumStar n=3 ['ra','dec','magG']  ← 另两份 xpsd_* n=5
   diag_gaia_psf_projection.py:210-218 / gate2_psf_oracle.py:158-166  argtypes 与实参均 8 个（C 侧 dpsf_fit_batch_f32 第 9 参 int *out_status）
   checks.json 引用: hips_direct_smoke.py=无, v5_maptile_oracle.py=无, hips_mapping_oracle.py=无,
                     v5_snr_precision_roundtrip.py=无, diag_gaia_psf_projection.py=无
$ python3 -B 问题扫描/_verify/_w6_final_numbers.py | sed -n '/W6-H/,$p'
   头文件分母: 203 | 匿名 typedef struct: 124 | 带 struct_size: 20 | 仅带 acs_head: 23 | 两者皆无: 81
```
C 侧逐家：`lib/astro_image_io/include/aio_hips.h:69/:84`、`lib/gaia_xpsd_client/src/gaia_client.h:57`、`lib/dynamic_psf/include/dynamic_psf.h:17/:38`、`lib/star_detector/include/star_detector.h:17` → `struct_size`/`abi_version` 0 命中；对照 `lib/plate_solve/cpp/ipv/include/ipv_api.h:78 IpvParams` 有（V2-N-01 账本 = FIXED / VERIFIED），同头 `:39 IpvWcsResult` 无。

**6）判"ARC-001 合同无机器消费者"（W6-N-05）**

```
$ git --no-optional-locks grep -ln module_dll_contract
→ README.md, REVIEW.md, docs/owner/{ARCHITECTURE_OVERVIEW,CHANGE_REVIEW,RELEASE_STATUS}.md,
  docs/review/{ARCHITECTURE_OVERVIEW,RELEASE_STATUS}.md, modules/conformance/noop/README.md
（.py / CMakeLists / ci 检查项：0）
$ git --no-optional-locks grep -ln 'nm -D\|readelf --dyn\|objdump -T' -- '*.py' '*.sh' '*.c' '*.cpp'
→ tests/abi/test_abi005_echo.py, tests/unit/drizzle_adapter_test.cpp, tests/unit/p1_hips/adapter_test.c
（ci/checks.json 内无"对交付 .so 比对导出白名单"的门命令）
```
Windows 判据：`tools/quality/ci_windows_driver.py:606+` 的 `exported_abi:<dll>` 步，其 ok 条件为 `res["exit_code"]==0 and bool(exports)`（非空即过）；`CMakeLists.txt:131/:137/:151/:228` 三平台 target 开 `WINDOWS_EXPORT_ALL_SYMBOLS ON`。**实际是否导出越界面需真机 dumpbin/nm，未判。**

**7）判"头引号 include 不可解析"（W6-N-07）**

```
$ python3 -B 问题扫描/_verify/_w6_hdr_decl2.py
口径 W6-Q2 公共头面分母: 69 | 符号: 508
=== A) 引号 include 悬空 ===
   runtime/registry/module_registry.h:6 -> runtime/module_loader/secure_loader.h     小计: 1
=== C) 系统整型头闭合 ===  小计: 8（见 W6-N-07 清单）
$ grep -n '^[a-zA-Z ]*include_directories(' CMakeLists.txt cmake/*.cmake   → 0 命中（无全局 -I）
```
`module_registry.h:6` 自称「本头独立可编译」；其两个消费者 `runtime/registry/module_registry.c`、`tests/abi/abi004_registry_probe.c` 均不在根图（见 §十-3）。

**8）判"ACR-DORMANT 锚点无生产者"（W6-N-08）**

```
$ git --no-optional-locks grep -n root-cmake
→ .github/workflows/ci-linux.yml:47（注释：V6.1 旧布局）; ci/steps/linux_build_root_graph.sh:16（注释）;
  cli/commands.cpp:228,237（历史兜底显示）; tools/check_duplication.py:22-24（三候选链）;
  tools/check_legacy_exit.py:5,19,60,67（单候选，无链）; tools/check_link_scan.py:9（单默认）;
  tools/check_reproducible_build.py:14,31（三候选链）; tools/check_warning_suppression.py:61-62,100（两候选链 + 显式 SKIP 打印）
$ grep -n 'build/' ci/steps/linux_build_root_graph.sh   → 产出 build/、build/cli、build/linux-openmp-on、build/linux-control；无 root-cmake 目录生产者
```
⇒ `bin_path.exists()`（`tools/check_legacy_exit.py:20/:68`）在 CI 恒假，`nm` 段整体跳过且无任何 SKIP 打印，末行仍 `LEGACY_EXIT_PASS`。

**9）判"`astrocs_p1_noise` 无定义点"（W6-N-09）**

```
$ git --no-optional-locks grep -n 'add_library(astrocs_p1_noise' -- '*CMakeLists.txt'   → 0 命中
$ python3 -B 问题扫描/_verify/_w6_v11b.py（片段 1）
   lib/snr_estimator/tests/p1noise/CMakeLists.txt:
     add_library -> ['p1noise_under_test','p5snr_under_test']
     add_executable -> ['p1noise_perf_test','p1noise_selfcheck_test','p1noise_tests','p1snr_science_test']
$ sed -n '1159,1170p' tests/unit/CMakeLists.txt
   if(TARGET astrocs_p1_noise) … add_test(NAME p1_noise_adapter …)   ← 第二条件永假
$ grep -n 'p1_noise_adapter' ci/known_failures.json ci/ctest_baseline.json
   ci/ctest_baseline.json:93  "p1_noise_adapter"
   ci/known_failures.json:52  reason: 「…门卫关闭时该 CTest 目标零注册零执行（当前状态），V7 残留（lib/snr_estimator untracked 面）收编后门卫自动重新激活…」
```

## 十、负结果（查过没找到的，防"没扫"被读成"没问题"）

1. **3 处"用了别处声明的符号而 include 闭包拿不到"经手验全部是假阳性**，不作为条目：`include/astrocs/io/aio_abi_v1.h`（`acs_status` 只出现在注释 :16-17、:64 行尾注释与 `_Static_assert` 消息串 :146/:158）、`lib/hips/include/astrocs/hips/publish.h`（:50/:69/:133/:142 同型）、`lib/snr_estimator/cpp/include/snr_estimator.h`（`HioSnrControlPoint` 只在 :352 注释与 :366 static_assert 消息里）。**方法学陷阱记录**：符号闭包比对必须先剥字符串字面量，否则 `_Static_assert` 的诊断文本会被当成引用。
2. 未发现任何"头文件独立编译/自包含"类 CI 门（`git grep 'fsyntax-only\|self-contained'` 命中面仅文档与 `artifacts/**`，无 `ci/`/`tools/quality` 消费者）⇒ 本轴的"能不能独立编译"只能静态推断，**是否真的编不过需运行期，未判**。
3. `runtime/module_loader/secure_loader.c`、`runtime/registry/module_registry.c` 在受跟踪 CMake 面 **0 引用**（`git grep -n 'secure_loader.c\|module_registry.c\|runtime/registry\|runtime/module_loader' -- '*CMakeLists.txt' '*.cmake'` → 空），仅被 `tests/abi/*.py` 用 `gcc` 手工编译（`tests/abi/test_abi005_echo.py:51,215-260`）。**本轴不另立条目**：其实质已由 M5b-G-06（产品不加载 / hash 全 null）与 M8-F-001（tests/abi 0 用例采集）覆盖，此处只补"编译面 0 引用"这一根因数字。
4. 安装树 unit 逐条路径比对：contract(16) ∩ product(10) 的 10 个共有 unit **install_path 与 rel_path 0 处不一致**；contract-only 6 条 = `LIC-CFITSIO/LIC-INDEX/LIC-NLOHMANN/SCHEMA-INSTALL-TREE/SCHEMA-PRODUCT/MANIFEST-PRODUCT`（属"文件类"而非"交付模块"，与 `astrocs.product.json` 只登记 runtime units 的设计一致）⇒ **不判缺失**。
5. `ci/workflow_binding.json`（18 步）与 `ci/validate_workflow_binding.py`（B1–B16）本轴按 B1/B4/B9/B13 四条独立复算未发现新违例；`ci/checks.json` 135 道门的 profile/平台分布与 `workflow_binding` 对账未发现"注册了但 workflow 不跑"的新站（既有站已由 M8-F-004/V19-N-06 覆盖）。
6. `.def`/`.map` 8 文件内容自洽（§五），未发现"def 列了 map 没列"类不一致；`lib/gaia_xpsd_client` 只有 visibility+宏本地化，无脚本面 —— 属设计差异，未判为缺陷。
7. 未找到 ARC-001 `module_dll_contract` 的**实例 JSON**（`git ls-files` + grep 双路）⇒ 该合同"只有 schema 无实例"是事实，但其影响已并入 W6-N-05，不单列。
8. `CMakePresets.json` 三 preset 与 `packaging/schemas/preset-contract.json` 的 preset 名（windows_formal/linux_light）本轴逐一比对**一致**，未发现 preset 名漂移；问题只在 schema 路径与校验器注册（W6-N-06）。
9. 未复现"checks.json 有门 ctest_targets 用了 glob 而 glob 匹配 0 名"——两个 glob（`p1snr_linux_*`、`p1snr_science_*`）实测各有 4/5 个真实名匹配，C4 不红 ⇒ 早先草稿里的"零命中"猜测作废。
