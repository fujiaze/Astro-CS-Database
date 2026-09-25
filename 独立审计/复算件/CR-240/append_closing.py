import io, sys
sys.stdout.reconfigure(encoding='utf-8')

path = r"独立审计/证据/通读-CR-240.md"
s = io.open(path, encoding="utf-8").read()

# 计数订正（表行数与清单行对齐）
s = s.replace("## §2.5 数值处置表（格 2，14 项）", "## §2.5 数值处置表（格 2，16 项）")
s = s.replace("## §2.5 数值处置表（格 5，16 项）", "## §2.5 数值处置表（格 5，18 项）")
s = s.replace("5. `lib/infrastructure/scheduler/src/module_adapters.cpp:1201-1500` —— 已读：是（表 16 项，findings 6 条）",
              "5. `lib/infrastructure/scheduler/src/module_adapters.cpp:1201-1500` —— 已读：是（表 18 项，findings 6 条）")

closing = r"""
---

# 收口

## 一、本批最高危三条（只挑，不复述）

1. **CR-240-18**（格 5）· P1 像素产品的发布序缺 `fsync(fd)`：`ASTROCS_DESIGN.md:688`、`docs/contracts/PUBLIC_API.md:1914`（R10-C 冻结序）与 aio 自身的 `aio_atomic_file.h:5-8`（"所有产品 = 临时文件/目录 + 校验 + fsync + 原子 rename 提交"）都要求 fsync 在 rename 之前，aio 的 JSON 原语照做了（`aio_atomic_file.h:120-146` 有 `_commit`/`fsync` 且失败即返码），但本文件的 FITS 面（`:1460-1474`＋`:1481-1495`）在 rename **之后**才 `fsync_parent_dir`，且该函数是 `void`（`aio_atomic_file.h:605-609`，内部 `(void)fsync_path(...)`）——本段唯一直接落在"正式目录只出现完整产品"这一主张上的洞。
2. **CR-240-07**（格 3）· 占位合同 ID 进产品：`ALG-002` 同时是 star-psf（`:888`）、wcs-platesolve（`:910`）、photometry（`:949`）三个节点的自报算法 ID，经 `:13175`→`commands.cpp:275` 落进 `run_manifest.provenance.algorithm_ids`，而 `ALG-002` 不在合同 ID 唯一事实源 `docs/contracts/INDEX.yaml` 内；同族 `ALG-P1-CAL-001`/`ALG-P1-COS-001` 亦然（CI 夹具 `eng/ci/fixtures/run_manifest/real_manifest_sample.json:50-53` 已在钉这三个值）。
3. **CR-240-15**（格 5）· 错型配置在节点 run 面静默回退默认（`:1354-1393`），而 P1 节点路径上没有任何 validate 面拦错型（`IModule::validate_config` 在 `lib/**` 零调用者，检索式见格 4"核过不报"），方向钉死为**放行**：字符串 `"false"` 对一个 `dflt=true` 的开关会被读成 true。与 `PUBLIC_API.md:721`「类型错→PARAM，无 silent default」相反。

## 二、我读不动或需要实跑才能定的

| 项 | 卡在哪 | 该跑什么（本批禁执行，未跑） |
|---|---|---|
| CR-240-18 的真实后果 | 静态读只能证"序里少一次 fsync"，不能证断电后会出现半写产品 | `strace -e trace=fsync,rename,openat` 跑一次 normalize 断言系统调用序；再做 SIGKILL/断电注入，比对 `calibrated_*`/`cleaned_*` 与 `p1_products.json` 登记 |
| CR-240-15 的受害键清单 | 各 `p1_op_*` 用 `p1_flag/p1_num/p1_int` 读的键与其 `dflt` 全在 :1500 之后 | 由 CR-241…CR-250 逐段登记 (键, 期望类型, dflt) 三元组，并与 `eng/contracts/schemas/phase_config_*.schema.json` 的 `properties`/`required` 做集合差；差集内每键补"错型必须 PARAM"的负例 |
| CR-240-10 的"哪一侧是科学正本" | P2 入端像素面到底是 ADU 还是 ADU/sr，取决于 normalize 写盘 BUNIT 与立体角归一在何处落进像素 | 读一份真实 P2 输入的 `properties`/`Moc.fits` 的 BUNIT，并与 `docs/science/DRIZZLE.md`＋`DATA_SEMANTICS §31.1` 对齐；必要时按 `实验/` 单元做一次量纲闭环复算 |
| 非 FITS（XISF）输入的完整性判据 | `bits_per_sample` 是否可能非 8 的倍数（12-bit 等）；XISF 头长与压缩使 `size >= header+raw` 是否根本不成立 | 读 `lib/infrastructure/aio/src/aio_xisf.cpp` 的 sample format 集合；或拿一份压缩 XISF 实跑 `p1_image_sane` |
| `aio_write_fits` 之外是否另有落盘屏障 | 我只证到 `git grep -n "fsync" -- lib/infrastructure/aio/src/aio_fits.cpp lib/infrastructure/aio/src/aio_api.cpp` 零命中；cfitsio 内部是否自行 flush 未穷尽 | 同上 `strace`；或读 aio 是否另有 `fits_flush_file`（`DATA_SEMANTICS.md:2413` 提到该符号存在） |
| `IModule::validate_config`/`inspect` 是否真无宿主 | `lib/**` 内零调用者已证，但编排面（Python/C ABI 宿主）不可静态见全 | 在生产二进制上查符号引用（`dumpbin /references` 或加一次性日志），确认这两个 vtable 槽是否可达；不可达则并入退役清单 |
| 288 KB 主头上界是否会误伤真实输入 | 现网/归档件里存在超 100 块主头的 FITS 与否，我没有样本证据 | 扫一遍 `testdata/` 与 BASS/HST 样本的 `NHDR` 块数分布（属实数据测量，本批禁跑） |

## 三、与同文件其它段的合并待办清单（CR-240…CR-250 共同定案）

本段只覆盖 1–1500 行。以下条目**不得由本批单方定案**：

1. **CR-240-02**（`aio_fs::walk_tree` 超深静默返回 0）：唯一调用点 `:8620` 在后续段 ⇒ 须确认该枚举是否服务于产品树清单/哈希；若是，本条按 S1/S2 重定级并补触发路径。
2. **CR-240-01/05 的下游一跳**：`lib/phase1_session/p1_session.cpp` 内 `ac_set_num_threads(host.budget.max_workers)` 的真实消费点；以及 `:15447/:15455` 的 `make_session_module<P2Api|P3Api>` 所注册 module_id 是否仍在生产 IR 里被引用——若已无引用，CR-240-04/05 的可达性由"生产中枢"降为"遗留壳"，定级随之下调。
3. **CR-240-07 的产品面**：`:13175`/`:13383`/`:15294`（节点 manifest 写 `algorithm_id`）、`:15461-15468`（`p1_nodes[]` 用哪些 descriptor）、`:14955`（字面量 `"ALG-P3-004"`）、`:15137`（`wr.value("algorithm_id", std::string())` ⇒ 空串是否会进 provenance）四处须逐段核。
4. **格 2 的 `kP1FitsPixelOrigin` 施加纪律**：`:4671` 一带与 `p1_tan_forward_reference_sip`（`:4425`）需核"恰好施加一次"；并核绝对交叉门的**门限本身**是否有科学依据（本批只证参考解数学独立、π 与 deg/rad 常数精确）。
5. **端口集双向差集**（CR-240-08/11/12/14 的合并面）：注册表有而 descriptor 无的端口——`p1_photoapplied`（phot 出／drz 入）、`p1_photscale`、`frame_hips`、`p1_products`、`p2_sky_plane`、`p2_final`、`p3_verify`、`run_context`；descriptor 有而注册表无的——`fluxes`(noise-snr 入)。须由读 `:1201` 之后各 `p*_op_*` 的段确认每条的真实读/写点，再决定改哪一侧。
6. **CR-240-14/16/17 的调用侧**：`p1_fits_saturation_level` 返回 0 时各 op 是否确实写 `DISABLED_NO_METADATA`（SCI NOISE_MODEL §4／claim SC-008 的义务）；`p1_calibrated_path` 回退到原帧时产品里有无任何降级声明；`p1_require_unique_frame_keys` 是否被每个按帧落位的 op 在循环前调用（漏一个则该 op 仍会互踩）。
7. **`:1193` `P1Api` 死壳**：与后续段共证 `p1_session_*` 是否在 :1500 之后另有真实调用者（本批检索式：`git grep -rn "P1Api"` 只有定义；`grep -n "p1_session_" 本文件` 只有 :265-269 声明与 :1194-1198）。若确认无 ⇒ CR-240-01 中属于 p1 的五条手抄声明随壳一起清理。
8. **跨段常量族**：`:1272/:1293` 的 `100ull*2880ull`（288 KB 主头上界）与 `:1346` 的非 FITS `2880` 下界，若后续段出现同值复写（HiPS/P3 读头处），按"同族同值合并"补进处置表；本批已把这二值列为 `待确认`。
9. **合并时须防"改对反而判红"**：`eng/ci/check_registry_ir_parity.py:46-58`（`PHASE1_ARTIFACT_TO_PORT` 手写字典）、`eng/tools/quality/check_block_flow_ports_vs_code.py`（注册表 code 锚点）、`eng/contracts/config/module_lifecycle_contract.schema.json:91`（required_callbacks 含 `validate_config`/`plan`）、`eng/ci/fixtures/run_manifest/real_manifest_sample.json:50-53`（钉 `ALG-002`/`ALG-P1-CAL-001`/`ALG-P1-COS-001`）、`eng/tools/quality/check_pipeline_graph.py:192,198`（钉 `parallel_axes`/`work_units` 字面量）——凡本批建议改 ID、删端口或改 plan 字段的条目，这六处夹具/门必须同批改。

## 收工 `git status --porcelain` 原文

```
?? ACSD整治工作包_AUDIT-06.zip
?? site/
```

与开工逐项一致（两项＝基线）。本批零 git 写、零 `git config` 写、未跑构建/ctest/任何 `eng/**` 脚本、未 import 仓库 Python、未在仓库目录内落任何文件；工作中间件只落 `独立审计/复算件/CR-240/`。

## 派单与清单差异

- 派单"1500 行分 5 格、每格约 300 行、骨架先写 5 行分段占位、PROGRESS 按 5 格计"与 `inventory/CR-240.txt`（唯一一行 `module_adapters.cpp:1-1500`）一致，无差异。
- 派单要求的接线轴五问落位：① 参数结构是否被消费 → CR-240-04/11/13；② 端口/单位/坐标系登记与两份合同同一套数 → CR-240-08/10；③ 缺键兜底方向 → CR-240-05/14/15/16/17；④ 被清零/丢弃的返回码与错误消息 → CR-240-03/06/17/18；⑤ 写死常数五档处置 → 五张 §2.5 表共 61 行（10+16+9+8+18）。
- 一处需要点明的口径差：本文件**没有**传统意义上的"节点构造参数表"，配置是以 `Json` 现取（`:1356-1393` 助手）＋descriptor 硬登记（`:692-1184`）两种方式并存；因此"参数结构是否真被下游消费"在本段的等价问法是"descriptor/plan 字段是否有读取者"，我按此执行并逐字段找了读取表达式。
"""

s = s.rstrip("\n") + "\n" + closing
io.open(path, "w", encoding="utf-8").write(s)
print("closing appended; table counts fixed")
