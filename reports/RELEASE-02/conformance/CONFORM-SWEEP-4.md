# CONFORM-SWEEP-4 —— Phase3 输出/投影/重采样 + 基建面 规范↔实现符合性审计

> 分片：**CONFORM-SWEEP-4**（AstroCS RELEASE-02 系统性缺陷类「规范写对了，实现没照做」整体排查）
> 审计范围（实现侧）：`lib/algorithms/fits_output/**`、`lib/algorithms/projection/**`、`lib/algorithms/resample/**`、
> `lib/infrastructure/scheduler/src/module_adapters.cpp` 的 p3_op_* 段、`lib/infrastructure/cli/**`、
> `lib/infrastructure/aio/**`、`lib/infrastructure/hips_browser/**`
> 规范侧（只读权威）：`docs/science/**`、`docs/algorithms/**`、`docs/contracts/**`、`docs/plugins/**`、
> `contracts/**`、`ASTROCS_DESIGN.md`、`docs/api/**`
> 纪律：只审计+登记，**未修改任何生产代码/文档**；零 git 写；未运行 ninja/cmake/ctest。
> 临时目录 `/dev/shm/astrocs_conf4`（审计结束后清理）。
> 每条含 `file:line` + 原文摘录；拿不准的标 `待定`/`规范歧义`，不把「符合」写成「不符」。
> 类别：C1 口径不符 / C2 常数不符 / C3 未接线 / C4 默认值不符 / C5 硬编码绕过 / C6 有实现无调用 / C7 合同不符；
> `另一类` = 规范本身错或规范之间打架（本轮要求一并上报）。

## 0a 审计快照与并发改动声明（必读）

- 审计期间（2026-09-19 19:0x–20:1x）工作树处于**并行改动**状态：`git status --porcelain` 共 40 项，其中与本分片直接相关的只有
  `lib/infrastructure/scheduler/src/module_adapters.cpp`（mtime 2026-09-19 19:28:37，+122/−40 行，全部落在 p1_op_photometry/p1_op_star_psf 等 **p1 段**，非本分片审计的 p3_op_* 段）。
  该改动使 p3_op_* 段整体**下移 +82 行**；本台账所有 `module_adapters.cpp` 行号已按改动后文件**重新基准并逐锚复核**（示例：output_mode 门 8058-8071→8140-8153；`prov.hips_id` 8447→8529；`p3_output_write_atomic_ex` 调用 8464→8546）。
  `docs/algorithms/PHASE3_RSMP_IMPL.md` 亦被并行任务改动（2 处行锚，行数不变 338），本台账对它的引用仍成立。
- 其余被引用文件（p3_output.cpp / p3_resample.cpp / p3_wcs.cpp / p3_rsmp_*.cpp / p3_v6_export.* / cli/** / contracts/** / docs/**）在审计期间**未被改动**。
- 快照哈希（sha256，用于复核审计基线）：见同目录 `CONFORM-SWEEP-4.json` 的 `snapshot` 字段；
  其中 `lib/infrastructure/scheduler/src/module_adapters.cpp` = `79efe106add6ca28162fb6c305608343c9df521429a0705b65c35ea5b207679f`（9042 行）。
- 结论：本台账行号对上述快照成立；若并行任务继续改动这些文件，行号需按 `git diff` 重新基准（内容结论不受影响）。

## 0c 未覆盖面（如实登记，子扫自述）

- aio 子扫：`aio_fits.cpp`（1187 行）仅核 BUNIT/BSCALE/BZERO/BITPIX/NaN 与写卡路径，未逐行核 fpack/omp/精度双轨全段；`aio_hips_reader.cpp`/`hiss_reader.cpp`/`hiss_writer.cpp`（含 SNR/诊断通道）、`aio_pipeline*.cpp`、`aio_api.cpp`、`aio_xisf.cpp`、`aio_compressor.cpp`、`ahpx/**`、`healpix_db/**`、`runtime/artifact_store/**` 未逐行核；`healpix/aio_healpix_io.cpp`（2197 行）仅核常量区、hiss/hcsd 头与子叶索引、读写入口；`aio/v6/**` 的 v6_fits.cpp / v6_atomic_publish.cpp / v6_sha256.cpp 未逐行核；`io/fits_core.c` 的 1 补码校验累加细节、`io/fits_verify.py`、`io/hips_output_store.py` 中段只抽样核；`contracts/**` 只核 v6 units/provenance/phase3 与 FITS/HIPS/BUNIT 相关冻结条款，未全量遍历。
- cli 子扫：见 partial-cli.md 自述（命令树/退出码/JSONL/模板/资源门为主；`process.cpp`、`memory_growth.h`、`monitor.h`、`resource_recorder.h`、`jsonl.h` 细节与 `normalize/**`、`mosaic/**`、`export/**` 子目录未逐行核）。
- core（本分片主审计）：`p3_proj_v6.cpp`（732 行）与 `p3_rsmp_failclosed.cpp`（216 行）只核注册表/门结构，未逐条复算 12 门；`p3_rsmp_operator.cpp` 的跨 tile 邻域几何未逐式复算；`p3_rsmp_propagation.cpp` 三模式只核 Q/W 主式。

## 0 汇总统计

- 规范陈述条数（登记条目总数）：**107**
- 符合 **41** / 不符 **37** / 未实现 **10** / 规范歧义 **13** / 待定 **6**

| 类别 | 条目 | 符合 | 不符 | 未实现 | 规范歧义 | 待定 |
|---|---|---|---|---|---|---|
| C1 | 21 | 0 | 13 | 0 | 5 | 3 |
| C3 | 12 | 1 | 2 | 8 | 1 | 0 |
| C4 | 11 | 0 | 6 | 0 | 4 | 1 |
| C5 | 4 | 0 | 4 | 0 | 0 | 0 |
| C6 | 2 | 0 | 1 | 0 | 0 | 1 |
| C7 | 12 | 0 | 6 | 2 | 3 | 1 |
| — | 40 | 40 | 0 | 0 | 0 | 0 |
| 另一类 | 5 | 0 | 5 | 0 | 0 | 0 |

> 说明：C2（常数不符）在本分片范围内未发现独立条目——范围内全部数值常数（2π、√(π/3)、kTileWidth=512、log2(W)=9、CRPIX=(W+1)/2、BUNIT 二次幂、max_tiles 公式、WCS roundtrip 容差、1e-12/1e-15 关键字对拍容差、0x9E3779B97F4A7C15 散列常数等）逐项回查均有规范出处且取值正确（见各「符合」条目）。

> 条目来源：core（本分片主审计）40 条；aio 子扫 26 条；cli/hips_browser 子扫 43 条；跨分片重复缺陷已合并 2 条（partial-cli-02→P3X-09, partial-cli-23→P3X-09, AIOX-01→partial-aio-002）。

## 0b 最严重的 5 条不符（本分片判定）

1. **P3X-06 [C6] 整个 V6 三模式 Phase3 导出链未编入生产二进制** —— CMakeLists.txt:718-719 的 astrocs_phase3_session 只含 p3_session.cpp；p3_v6_export.cpp（801 行）仅测试编译，p3_proj_v6.cpp 无 target，p3rsmp:: 全命名空间零生产调用者；而 FZ-P3-MODES / FZ-P3-QW-RECOMPUTE / FZ-P3-BUNIT-QUADRATIC / FZ-P3-KERNEL-REGISTRY 四条 FROZEN 合同要求的能力全在该链上。影响：合同在生产运行面无载体，且 PRODUCTION_EXECUTION_INVENTORY.csv:335 反标 production=yes。
2. **P3X-05 [C3] FZ-P3-MODES 三输出模式中两条在生产被拒** —— CLI 模式门 v6_runtime_contract.h:130-138 把 surface_brightness/point_source_flux/visualization 全判 kProduction 放行，而 p3_op_resample（module_adapters.cpp:8140-8153 + p3_resample.cpp:212-221）只接受 surface_brightness，其余以 DATA 失败。影响：point_source_flux（Q/W 输出帧重算、FLUX/EFFECTIVE_PSF HDU）与 visualization 不可达；同一条生产路径前门放行、后门拒绝。
3. **P3R-06 [C1] leaf signal 为 NaN 时 variance/ivar 未同态置 NaN** —— DATA_SEMANTICS.md:2557 要求该行三平面皆 NaN；module_adapters.cpp:8289+8309-8315（同 p3_session.cpp:293+321-328）在 signal=NaN 时仍写有限 var=Σc²u 与 ivar=1/var。影响：无信号像素携带有限方差，按 ivar 加权/按 variance 判有效的下游会把它当有效测量；VARIANCE HDU 与 SIGNAL HDU invalid 语义不同态。
4. **P3R-08 [C1] bilinear 退化填充使同一 leaf 占多角 ⇒ 方差按重复列平方求和而低估** —— p3_resample.cpp:369-371 用最近点填充空象限，:397-403/:500-509 对重复 leaf 分别平方累加；正确应为 (Σw)²·u。影响：边界/退化像素 VARIANCE/IVAR 最多低估 4×（σ 2×），且该低估不在 UNCERTAINTY_AND_COVARIANCE.md 已登记的「对角特例下界」范围内。
5. **P3F-08 [C5] provenance 的「源 HiPS 标识」是硬编码常量** —— SCI-P3 §9a-11 要求写源 HiPS 标识；module_adapters.cpp:8529 与 p3_session.cpp:373 恒写 ivo://astrocs/phase3，HISTORY 随之写 source=ivo://astrocs/phase3（p3_output.cpp:228-230）。影响：产物无法回答由哪个输入 HiPS 导出（manifest hash 仅能区分内容）。

> 紧随其后（同为本分片判定，建议同批处理）：**partial-cli-12**（-force 绕过结构检查，配置类型错 → export rc=70 崩溃，normalize/mosaic 同输入 rc=2）、**P3X-08**（signal 面亮度单位两链冲突 ADU vs ADU/px²）、**P3X-09**（export 配置合同与生产平铺键完全不兼容，合同模板实测 rc=3）、**P3F-09**（verify 不回读 BUNIT/不校验 DATASUM/CHECKSUM）。


## A. fits_output（Phase3 FITS 写出域）

### P3F-01 [—] FITS 头关键字面（BITPIX/BSCALE/BZERO/BUNIT/WCS/HISTORY）逐项按 SCI-P3 §9a-11 写出
- 规范：`docs/science/PHASE3_HIPS_TO_FITS.md:148`「`BITPIX=-32/-64`；`BSCALE=1,BZERO=0`；`BUNIT` 按 properties（缺省 'ADU'）；WCS=`CRPIX/CRVAL/CD1_1,1_2,2_1,2_2/CTYPE=TAN/CUNIT=deg`；`HISTORY+provenance`（源 HiPS 标识/order_sel/sampler/软件版本/manifest hash）必写。」
- 实现：`lib/algorithms/fits_output/p3_output.cpp:186-231`
- 判定：符合
- 证据：p3_output.cpp:186-187 写 CTYPE1/2=RA---TAN/DEC--TAN；:189-190 CUNIT=deg；:197-204 CRPIX/CRVAL/CD1_1..CD2_2 全量 TDOUBLE；:212-214 BSCALE=1.0/BZERO=0.0；:215-216 BUNIT=bunit?:'ADU'；:219-230 HIPSID/RUNID/ORDERSEL/SAMPLER/SWVER + fits_write_history。
- 影响：FITS 头关键字面与 SCI-P3 §9a-11 逐项一致。
- 修复面：无（符合）（只登记，不改）

### P3F-02 [—] coverage 二值门 covered⇔value>0.5f 写读一致
- 规范：`docs/contracts/DATA_SEMANTICS.md:2108`「coverage | float32 | [W·H] | DIMENSIONLESS（二值门 {0,1}） | covered ⇔ value>0.5f」
- 实现：`lib/algorithms/fits_output/p3_output.cpp:376,495,544`
- 判定：符合
- 证据：写路径 :376 `if (coverage[i] > 0.5f) ++cov;`；verify 回环 :495 `if ((cov[i] > 0.5f) != (coverage[i] > 0.5f)) { covok = 0; break; }`（先二值化再比）；:544 同。
- 影响：coverage 二值语义与 DATA-P3-FITS §27 一致；浮点 0.7/0.9 等价。
- 修复面：无（符合）（只登记，不改）

### P3F-03 [—] VARIANCE/IVAR 的 BUNIT 二次律 'ADU^2' / '1/(ADU^2)' 与合同逐字一致
- 规范：`docs/contracts/DATA_SEMANTICS.md:2566`「BUNIT=<signal BUNIT>^2 / 1/(<signal BUNIT>^2)（缺省 ADU → "ADU^2" 与 "1/(ADU^2)"）」
- 实现：`lib/algorithms/fits_output/p3_output.cpp:282-296`
- 判定：符合
- 证据：`std::string var_bunit = std::string(unit) + "^2"; std::string ivar_bunit = std::string("1/(") + unit + "^2)";` 且 :294-296 逐 HDU 写 BUNIT。unit 缺省 'ADU'（:282 `(bunit && *bunit) ? bunit : "ADU"`）⇒ 'ADU^2' / '1/(ADU^2)'。
- 影响：方差/逆方差单位二次律成立。
- 修复面：无（符合）（只登记，不改）

### P3F-04 [—] 原子发布序 flush→close→fsync→rename，任一步失败不发布
- 规范：`docs/algorithms/PHASE3_FITS_IMPL.md:185-190`「**F1 原子发布序**（IO_003 §4/§6 实现，R10-C 修正 :221-224）: tmp 建写 → fits_flush_file（cfitsio dirty buffer 全量到 OS，失败即无成功对象 :231-236）→ close :237-239 → fsync(fd)（POSIX O_RDONLY /Windows O_RDWR _commit :240-267）→ rename :269-273。任何一步失败 → unlink(tmp) 不发布」
- 实现：`lib/algorithms/fits_output/p3_output.cpp:316-362`
- 判定：符合
- 证据：:320-325 fits_flush_file 失败→close+unlink+IO；:326-327 fits_close_file 失败→unlink+IO；:335-355 open+fsync+close 三处失败均 unlink+IO；:358-362 rename 失败→unlink(tmp)+IO。
- 影响：崩溃/失败不留可见半成品，符合 IO_003。
- 修复面：无（符合）（只登记，不改）

### P3F-05 [—] sha256 仅在完整读出后产出 64hex，失败即整体失败
- 规范：`docs/algorithms/PHASE3_FITS_IMPL.md:197-199`「**F2 完整性锚**（:85-91/:92-114）: sha256 仅在文件完整读出后产出 64hex；空串/前缀哈希禁止入 result/provenance」
- 实现：`lib/algorithms/fits_output/p3_output.cpp:96-112,368-372`
- 判定：符合
- 证据：:103-106 逐块 fread + ferror/fclose 全检查，任一失败 return false；:368-372 发布后哈希失败 → unlink(output_path) 并返回 P3_OUT_IO，不写假哈希。
- 影响：无完整性锚的产物不发布。
- 修复面：无（符合）（只登记，不改）

### P3F-06 [—] DATASUM/CHECKSUM 走 cfitsio fits_write_chksum 逐 HDU
- 规范：`docs/algorithms/PHASE3_FITS_IMPL.md:88-91`「标准校验和（B2-A9）: PRIMARY 在写 signal 后调 `fits_write_std_chksum`（:241-249，cfitsio `fits_write_chksum` → DATASUM+CHECKSUM 逐 HDU），COVERAGE :263-271、VARIANCE/IVAR :294-302 同」
- 实现：`lib/algorithms/fits_output/p3_output.cpp:67-74,246-254,268-276,300-306`
- 判定：符合
- 证据：:69 `fits_write_chksum(f,&status)`；四个 HDU 分支各调一次且失败即 close+unlink+IO。
- 影响：DATASUM/CHECKSUM 由 cfitsio 标准实现按 HDU 归属。
- 修复面：无（符合）（只登记，不改）

### P3F-07 [—] verify 回环 NaN==NaN 视为一致
- 规范：`docs/algorithms/PHASE3_FITS_IMPL.md:203-206`「**F4 NaN 回环语义**（:327-330）: 双方 NaN 视为一致（源无覆盖=NaN 传播），否则逐值精确相等」
- 实现：`lib/algorithms/fits_output/p3_output.cpp:476-481,523-527`
- 判定：符合
- 证据：:478-480 `const bool sn=(signal[i]!=signal[i]); const bool rd=(sig[i]!=sig[i]); if (sig[i]!=signal[i] && !(sn&&rd)) { ok=0; break; }`；uncertainty 面 :524-526 同构。
- 影响：NaN 覆盖像素不误判为回环失败。
- 修复面：无（符合）（只登记，不改）

### P3F-08 [C5] provenance 的「源 HiPS 标识」被写成编译期常量 ivo://astrocs/phase3
- 规范：`docs/science/PHASE3_HIPS_TO_FITS.md:148`「`HISTORY+provenance`（**源 HiPS 标识**/order_sel/sampler/软件版本/manifest hash）必写」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:8529（同 lib/phase3_session/p3_session.cpp:373）`
- 判定：不符
- 证据：`prov.hips_id = "ivo://astrocs/phase3";` 为编译期常量，与输入 HiPS 产品无关；HISTORY 由 p3_output.cpp:228-230 写成 `HISTORY phase3 source=ivo://astrocs/phase3 manifest=<真实哈希>`。全仓 grep `ivo://astrocs` 显示 Phase1/2 亦用固定 creator_did（aio_hips_writer.cpp:634、module_adapters.cpp:7248），即该字段在三条链上都是模块常量而非源产品标识。
- 影响：产物的「源 HiPS 标识」字段恒为同一常量：同一 FITS 无法回答它由哪个输入 HiPS 导出（manifest hash 仍可区分内容，但 HIPSID/source 字段不可用于溯源查询）。
- 修复面：module_adapters.cpp:8529 与 p3_session.cpp:373 改为从输入产品读实际标识（HiPS properties 的 creator_did/ID 或 CLI run_context 的输入产品 ID）；p3_output.cpp:228-230 HISTORY 的 source= 随之变为真实值。只登记不改。（只登记，不改）

### P3F-09 [C3] verify 不回读 BUNIT、不校验 DATASUM/CHECKSUM（合同 T2 要求）
- 规范：`docs/algorithms/PHASE3_FITS_IMPL.md:292-293`「T2 独立 verify：重开 dims/WCS/**BUNIT/checksum**/mask 一致 → reopen_ok=1、coverage_ok=1、sha256 64hex（:89-100）。」
- 实现：`lib/algorithms/fits_output/p3_output.cpp:441-538`
- 判定：不符
- 证据：verify_ex 的读回面 = CTYPE1/2、CUNIT1/2（:441-452）、CRPIX/CRVAL/CD（:454-467）、HDU1 像素回环（:468-483）、HDU2 coverage 二值门（:486-497）、uncertainty EXTNAME/尺寸/像素（:500-538）。**全文无 fits_read_key BUNIT，无 DATASUM/CHECKSUM 读回或校验**（grep 'BUNIT|DATASUM|CHECKSUM' 在该文件零命中；只有写路径 :216/:294）。§2 符号表把 BUNIT 列为冻结关键字，§12 T2 声明 verify 校验 BUNIT/checksum。 旁证（措辞有歧义，不作决定性依据）：p3_output.h:64-65 声明 verify「读回 header 数字+数据回环(用 fits_read_file)并重算 checksum」，实现仅重算 sha256（:548-556），无任何 FITS DATASUM/CHECKSUM 校验调用。
- 影响：BUNIT 写错（如 'Jy/beam'）或校验和被改写都不会被进程内 oracle 检出：writer 节点 manifest 仍报 reopen_ok=1，验收门失去鉴别力。
- 修复面：p3_output.cpp:441-467 之后增加 fits_read_key(TSTRING,"BUNIT") 与 descriptor/期望值对拍，并加 fits_verify_chksum 对每个 HDU 的 DATASUM/CHECKSUM 校验；任一不符置 wcsok=0/新标志位。只登记不改。（只登记，不改）

### P3F-10 [C4] 主 HDU 与 VARIANCE HDU 的 BUNIT 空串缺省判定不一致
- 规范：`docs/science/PHASE3_HIPS_TO_FITS.md:148`「`BUNIT` 按 properties（**缺省 'ADU'**）」
- 实现：`lib/algorithms/fits_output/p3_output.cpp:215 vs :282`
- 判定：待定
- 证据：主 HDU：`const char* unit = bunit ? bunit : "ADU";`（仅判空指针，空串 "" 会原样写入空 BUNIT）；uncertainty HDU：`const char* unit = (bunit && *bunit) ? bunit : "ADU";`（空串回退 ADU）。同一函数的两个 BUNIT 缺省判定不一致。生产链路目前不可达：module_adapters.cpp:8549 传 `res.value("bunit","ADU")`，而 p3_resample.cpp:286 已把空 properties bunit 归一为 "ADU"。
- 影响：经公共 API 直调（bunit=""）时主 HDU 写空 BUNIT 而 VARIANCE 写 'ADU^2'，单位面自相矛盾；生产 CLI 路径当前不触发。
- 修复面：p3_output.cpp:215 改为与 :282 同款 `(bunit && *bunit) ? bunit : "ADU"`。只登记不改。（只登记，不改）

### P3F-11 [OTHER] PHASE3_FITS_IMPL.md 的行号锚与行数与实测不符（556 vs 560 等）
- 规范：`docs/algorithms/PHASE3_FITS_IMPL.md:10-11`「模块: lib/algorithms/fits_output/p3_output.cpp（556 行，B2-A9 后实测）+ 唯一权威签名头 lib/algorithms/fits_output/p3_output.h（64 行）」
- 实现：`lib/algorithms/fits_output/p3_output.cpp（实测 560 行）、p3_output.h（实测 99 行）`
- 判定：不符
- 证据：`wc -l` = 560 / 99，文档声明 556 / 64。连带行锚漂移（逐条实测）：§2 表 signal 锚 :212-214 实为 BSCALE/BZERO 块（signal 写入在 :236）；§3 `make_temp_path（:72-84）` 实为 :76-87，`tmp=…` 实为 :84（文档 :81）；§3 `p3_output_verify_ex（:392-541）` 实为 :396-558；§4 伪代码 verify 参数门 :297-299 / fits_open_file :312 实为 :401 / :414。§3 中段锚（头包含 :47-54）与实测一致 ⇒ 漂移非整体平移，而是部分锚未随迁移/B2-A9 更新。
- 影响：文档自称「逐符号源码行号锚定」，审计/整改按锚定位会落到错误行（本次审计已实际遇到）；不影响运行时行为。
- 修复面：按实测重刷 PHASE3_FITS_IMPL.md 的行锚与行数（属规范侧文档缺陷，另一类）。只登记不改。（只登记，不改）

### P3F-12 [C3] tmp 命名偏差（DISP-P3FITS-002）已登记，另有未使用的 hostname 死代码
- 规范：`docs/algorithms/PHASE3_FITS_IMPL.md:337-343`「**DISP-P3FITS-002**：tmp 命名冻结注与实现偏差——p3_output.h:41-44 协议注写 `<dir>/.<base>.<pid>.tmp`（前置点隐藏文件形态），实测 make_temp_path 生成 `out_path.<pid>.tmp`」
- 实现：`lib/algorithms/fits_output/p3_output.cpp:76-87`
- 判定：符合
- 证据：实现 `*tmp = out + "." + std::to_string(::getpid()) + ".tmp";`（:84）与 DISP-P3FITS-002 登记的实测形态一致；`char host[64]`/gethostname（:77-83）计算后未使用（死代码，规范未提及）。 行锚复核：文档称该协议注在 p3_output.h:41-44，实测在 p3_output.h:50-53（「写入 <dir>/.<base>.<pid>.tmp → flush(cfitsio 缓冲全部写出) → fsync(fd) → rename」），与 P3F-11 同源的锚漂移。
- 影响：已登记偏差，同目录 rename 原子性不受影响。
- 修复面：（已登记）建议清理 :77-83 未使用的 hostname 计算，并统一 h:41-44 协议注。只登记不改。（只登记，不改）

## B. projection（Phase3 投影/WCS 域）

### P3P-01 [—] G1 输出 WCS 构造（CRPIX/CD/parity）与 astropy 独立对拍一致
- 规范：`docs/algorithms/PHASE3_RESAMPLE.md:16-19`「G1 (ALG-P3-002) 输出 WCS 构造 (FITS 1-based, CD-only): CRPIX1=(W_out+1)/2, CRPIX2=(H_out+1)/2, CRVAL=center; |CD1_1|=|CD2_2|=s_out, CD1_2=CD2_1=0; east_left: CD1_1=−s_out, CD2_2=+s_out; east_right: CD1_1=+s_out, CD2_2=−s_out」
- 实现：`lib/algorithms/projection/p3_wcs.cpp:106-140`
- 判定：符合
- 证据：:108-109 crpix=(W+1)/2.0/(H+1)/2.0；:132-140 sgn_x(east_left=-1,east_right=+1)、sgn_y=-sgn_x、PA=0 时 cd=diag(sgn_x·s, sgn_y·s)（east_right 得 diag(+s,-s)，与 G1 一致）。独立 oracle：以 astropy 7.0.1 WCS(TAN, CD, CRPIX=(W+1)/2) 对拍同一 descriptor，4 组 (W,H,center,scale,parity) 网格 max|impl−astropy| = 2.1e-14 / 8.5e-14 / 1.4e-13 deg（脚本 /dev/shm/astrocs_conf4/wcs_check.py，复算 LONPOLE 默认由 astropy 施加）。
- 影响：输出网格几何与 FITS WCS 标准（含 LONPOLE 默认）一致，无 1px 原点偏移。
- 修复面：无（符合）（只登记，不改）

### P3P-02 [—] G2 反向映射 CD·((x+1)−CRPIX)+TAN 去投影与 astropy 一致
- 规范：`docs/algorithms/PHASE3_RESAMPLE.md:21-27`「G2 (ALG-P3-002) 反向映射 (逐输出像素 (x,y), 1-based→中间平面): (iwc1,iwc2) = CD · ((x+1)−CRPIX1, (y+1)−CRPIX2) ... world = proj^{-1}(iwc; CRVAL)」
- 实现：`lib/algorithms/projection/p3_wcs.cpp:154-179`
- 判定：符合
- 证据：:158-159 `dx = fits_pixel_1based(x) - d->crpix_x`（全文件唯一 +1 桥接，:46/:49-56）；:160-161 ξ,η = CD·δ；:164-177 标准 TAN 去投影（θ=atan2(1,r)，φ=atan2(−ξ,η)，dec=asin(sinθ sinδ0+cosθ cosδ0 cosφ)，Δα=atan2(−cosθ sinφ, cosδ0 sinθ−sinδ0 cosθ cosφ)）与 Paper II §2.2 一致。astropy 对拍同上（P3P-01）。
- 影响：反向映射口径正确；RA wrap 经 fmod 归一（:26-29）。
- 修复面：无（符合）（只登记，不改）

### P3P-03 [C3] 八投影冻结集合仅实现 4/8，且 v3 registry 未编入生产 target
- 规范：`docs/science/PHASE3_HIPS_TO_FITS.md:132-136`「**registry 冻结集合 = ASTROCS_DESIGN §5.3 八投影**（TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA），v3 已实现 4/8（STG/MOL/CEA/ZEA 归 P3-001）。会话面与 registry 面分离：注册面已登记、alpha 会话未接线」
- 实现：`lib/algorithms/projection/p3_proj_v6.h:58-73（v3 registry）、lib/algorithms/projection/CMakeLists.txt:13-15`
- 判定：未实现（规范已登记为会话面收窄）
- 证据：v3 ProjectionId 仅 {kTAN,kSIN,kCAR,kAIT}（4/8）；`astrocs_p3_projection_wcs` 仅编译 p3_wcs.cpp（CMakeLists.txt:15），p3_proj_v6.cpp 无生产 target（CMakeLists.txt:13 注「target 声明归 P3-002 线（本批次未动）」）。v1 registry（p3_projection.h:4-9）已 RETIRED。生产 CLI 唯一接受 TAN（p3_wcs.cpp:63-69）。
- 影响：STG/MOL/CEA/ZEA 不可用；TAN 之外的已实现 4 投影在生产不可达。规范已按 §9a-3/ALG §15.1/§15.5 登记为有意收窄，非新增缺陷。
- 修复面：（已登记）P3-002 线补 p3_proj_v6 target 声明与会话接线；STG/MOL/CEA/ZEA 补齐。只登记不改。（只登记，不改）

## C. resample（Phase3 重采样/不确定度传播域）

### P3R-01 [—] order 选择线性扫描与 G3 冻结式数学等价
- 规范：`docs/algorithms/PHASE3_RSMP_IMPL.md:120-131`「冻结语义：在线性扫描 `k ∈ [0, max_order]` 中找**最小** k 使 `pixel_resolution_arcsec(nside=512 << k) / 3600 ≤ scale_deg_per_px` … 与 ALG-P3-003 G3 冻结式 … **数学等价**」
- 实现：`lib/algorithms/resample/p3_resample.cpp:199-210`
- 判定：符合
- 证据：实现即该线性扫描（:203-207 `res_deg = pixel_resolution_arcsec(512u<<k)/3600.0; if (res_deg <= scale) {*out_order=k; return OK;}`），扫描完未命中 → *out_order=max_order（:208-209，= clamp 到 hips_order 的欠采样降级）。权威函数 healpix_core.cpp:327-331 `sqrt(4π/(12·nside²))·180·3600/π` = `sqrt(π/3)/nside` rad，与 ALG §6.1 同式。
- 影响：order_sel 与 SCI-P3 §5/§9a-5 冻结公式等价。
- 修复面：无（符合）（只登记，不改）

### P3R-02 [—] nearest/bilinear 的 NaN 与 coverage 值语义与 SCI-P3 §5 真值表一致
- 规范：`docs/algorithms/PHASE3_RSMP_IMPL.md:204-209`「| tile 存在，像素 NaN | NaN（传播，非错误） | 1 | / | tile 缺失/读失败 | NaN | 0 |」
- 实现：`lib/algorithms/resample/p3_resample.cpp:294-306,374-380`
- 判定：符合
- 证据：nearest：:302 读失败 → `*value=nanf(""), *coverage=0`；:303 读成功直传（NaN 亦直传）+ :304 `*coverage=1`。bilinear：:378-380 任一角缺失 → NaN/C=0；:393-396 any_nan → value=NaN，`*coverage = 1`。
- 影响：coverage 与 NaN 值语义与 SCI-P3 §5 真值表一致。
- 修复面：无（符合）（只登记，不改）

### P3R-03 [—] ivar==0 → NaN 传播态；负/Inf → 产品损坏显式拒（禁 1/0→Inf）
- 规范：`docs/contracts/DATA_SEMANTICS.md:2516-2526`「u_in = 1/ivar（**ivar==0 像素 = 零权重 ⇒ u 无效 = NaN 传播态，非硬错误**…）；u 值域: NaN = 传播态…；负/Inf…= **产品损坏 → 显式错误**」
- 实现：`lib/algorithms/resample/p3_resample.cpp:470-487`
- 判定：符合
- 证据：:479-481 `if (d==0.0 && impl->unc_src==P3_UNC_IVAR) { *out=nanf(""); *st=P3_U_NAN; return OK; }`；:482 `if (d<0.0 || !std::isfinite(d)) return P3_RS_PARAM;`（variance 的 +Inf 与 ivar 的 +Inf 都拒）；:483-484 ivar 取倒数后非有限亦 PARAM（禁 1/0→Inf 导出）。
- 影响：产品损坏与像素级零权重严格区分，符合 M1a-A-009 订正。
- 修复面：无（符合）（只登记，不改）

### P3R-04 [—] 方差传播按对角特例 Σc_k²u_k（规范已登记为下界口径）
- 规范：`docs/contracts/DATA_SEMANTICS.md:2542-2548`「bilinear:  C_in = diag(u_k) ⇒ var_out = Σ_k c_k² · u_k   # 对角特例（不是通用式） … ivar_out = 1 / var_out   (var_out 有限且 >0) / ivar_out = var_out 同态  (var_out=0 → 0 显式不可用; NaN → NaN)」
- 实现：`lib/algorithms/resample/p3_resample.cpp:498-516 与 lib/infrastructure/scheduler/src/module_adapters.cpp:8309-8315`
- 判定：符合（实现口径=对角特例，规范已如实登记）
- 证据：:508 `acc += w*w*u_k;`（nearest npts=1 时 :507 w=1.0 ⇒ var=u，即 W1 特例）；op 侧 :8311-8315 `u_out>0 ? 1/u_out : (u_out==0 ? 0.0f : NaN)` 与 :2547-2548 逐条一致。docs/science/UNCERTAINTY_AND_COVARIANCE.md:100-102 已登记「现按对角特例 Σc_k²u_k 传播（不建完整 C_in）；该口径是**下界**」。
- 影响：与登记一致；但完整式 R C_in Rᵀ（FZ-FORMULA-COV-PROP）在生产未实现——见 P3X-13。
- 修复面：无（符合登记口径）（只登记，不改）

### P3R-05 [—] invalid 表四行在两条生产路径上逐行一致
- 规范：`docs/contracts/DATA_SEMANTICS.md:2554-2559`「| 无覆盖（tile 缺失/足迹无 tile 像素） | NaN | NaN | NaN | 0 | / | 覆盖不一致（leaf signal 有限而 u 无效/缺失） | 按采样值 | NaN | NaN | 1（+ provenance uncertainty_missing_pixels 计数） |」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:8289-8315（同 lib/phase3_session/p3_session.cpp:293-328）`
- 判定：符合
- 证据：无覆盖：:8289-8290 sig=NaN/cov=0，:8292-8295 var=ivar=NaN；u 缺失：:8303-8307 missing_px++ 且 var=ivar=NaN、signal 保持采样值、cov=1。
- 影响：两条生产路径对 invalid 表的处理逐行一致（除 P3R-06/P3R-07 两处）。
- 修复面：无（符合）（只登记，不改）

### P3R-06 [C1] leaf signal 为 NaN 时 variance/ivar 未同态置 NaN（违反 invalid 表 NaN 传播行）
- 规范：`docs/contracts/DATA_SEMANTICS.md:2557`「| NaN 传播（**足迹内 leaf signal** 或 u 为 NaN） | NaN | **NaN** | **NaN** | 1 |」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:8284-8315（同 lib/phase3_session/p3_session.cpp:288-328）`
- 判定：不符
- 证据：当采样到的 leaf signal 为 NaN（bilinear any_nan / nearest 直传 NaN）而 u 有限时：:8289 sig=NaN、cov=1，随后 :8300 正常传播，:8309-8315 得 var_plane=u_out（有限）、ivar=1/u_out（有限）。规范该行要求 variance/ivar 同为 NaN。两条生产路径同款（p3_session.cpp:321-328 同）。
- 影响：signal=NaN 的像素携带有限 variance/ivar：下游按 ivar 加权或按 variance 判有效的消费者会把「无信号」像素当作有效测量（文档自述 mask 语义 = coverage ∧ isfinite(signal)，即要求消费者另算掩膜）；VARIANCE/IVAR HDU 与 SIGNAL HDU 的 invalid 语义不同态。
- 修复面：module_adapters.cpp:8309 与 p3_session.cpp:321 之前增加 `if (!std::isfinite(sig[i])) { var=NaN; ivar=NaN; continue; }`（或对 bilinear 任一 c_k 值 NaN 置同态）。只登记不改。（只登记，不改）

### P3R-07 [C1] u 无效（ivar==0/NaN）未计入 provenance uncertainty_missing_pixels（规范两行重叠）
- 规范：`docs/contracts/DATA_SEMANTICS.md:2558`「| 覆盖不一致（leaf signal 有限而 u 无效/缺失） | 按采样值 | NaN | NaN | 1（+ provenance **uncertainty_missing_pixels 计数**，不中断不补 0） |」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:8303-8307`
- 判定：待定
- 证据：`missing_px` 仅在 `u_st == P3_U_MISSING`（u 子产品缺该 leaf）时自增；`u_st == P3_U_NAN`（ivar==0 或 u 值 NaN，§30.4-1 明称「u 无效」）走 :8309-8315，**不计数**。§30.4 第 2557 行「u 为 NaN」与第 2558 行「u 无效」两行触发条件重叠（iv=0 的 u 无效态就是 NaN 传播态），规范自身对「signal 有限 ∧ u=NaN」应落哪一行未唯一确定。
- 影响：若按行 2558 读，provenance 的 uncertainty_missing_pixels 会系统性少计 ivar==0 像素数（诊断/验收计数偏低）；若按行 2557 读，则还要求 signal=NaN（与实现相反）。判定留待规范消歧。
- 修复面：先由规范侧消解 §30.4 invalid 表两行重叠（建议把「u 无效=ivar==0/NaN」并入行 2558 并在行 2557 只保留 leaf signal NaN）；随后 module_adapters.cpp:8309 与 p3_session.cpp:321 增计数。只登记不改。（只登记，不改）

### P3R-08 [C1] bilinear 退化填充导致同一 leaf 占多角 ⇒ 方差按重复列平方求和而低估
- 规范：`docs/contracts/DATA_SEMANTICS.md:2542`「bilinear:  C_in = diag(u_k) ⇒ var_out = Σ_k c_k² · u_k（R = ALG-P3-003 G4 冻结权重行向量（Σ_k c_k = 1）；C_in = 输入逐像素协方差阵）」
- 实现：`lib/algorithms/resample/p3_resample.cpp:369-371,390-392,397-403,500-509`
- 判定：不符
- 证据：退化防护把无邻域点的象限用 `q[i][j] = nearest_pt`（:369-371）填充，**同一个 leaf 可占据 2–4 个角点**；:397-403 仍把 4 个（可能重复的）leaf 与其权重分别导出，:500-509 逐 k 累加 `w_k²·u_k`。方差二次型的行向量 R 是对**不同输入像素**的权重：同一 leaf 出现两次时正确贡献是 (w_a+w_b)²·u，实现为 w_a²·u+w_b²·u ⇒ 低估。极端情形 4 角同 leaf 且权重均 1/4：正确 var=u，实现 var=(1/16)·4·u=u/4（低估 4×）。
- 影响：tile/面边界与角点退化像素的 VARIANCE/IVAR 被低估（最多 4×，σ 低估 2×），且该低估未在 UNCERTAINTY_AND_COVARIANCE.md 的下界声明范围内（该声明只针对 C_in 非对角，不针对重复列）。
- 修复面：p3_sample_bilinear_ex 在填充后合并重复 leaf（把同 ipix 的权重相加后输出，npts 随之 <4），或 p3_uncertainty_propagate 先按 ipix 归并权重再平方累加（p3_resample.cpp:397-403 + :500-509）。只登记不改。（只登记，不改）

### P3R-09 [C1] p3_resample_check_mode 对 "flux"/"flux_per_pixel" 返回 PARAM 而非 UNSUPPORTED
- 规范：`docs/algorithms/PHASE3_RSMP_IMPL.md:141-143`「`input_mode == "surface_brightness"` 唯一返回 `P3_RS_OK`；其余（含 nullptr/空串/`flux`/`flux_per_pixel`/variance/weight/ivar）返回 `P3_RS_UNSUPPORTED`」
- 实现：`lib/algorithms/resample/p3_resample.cpp:212-221`
- 判定：不符
- 证据：实现：`if (m=="weight" || m=="flux-per-pixel") return P3_RS_UNSUPPORTED; if (m=="surface_brightness") return P3_RS_OK; return P3_RS_PARAM;`（:218-220）。故 `"flux"`、`"flux_per_pixel"`（下划线形态）、nullptr、空串均返回 PARAM(1) 而非 UNSUPPORTED(2)；variance/ivar 归 PARAM 系 DATA-UNC-001 §30.4-4 supersession（p3_resample.h:20-26 已同步），但 `"flux"`/`"flux_per_pixel"` 属未同步的纯偏差。
- 影响：拒绝行为本身成立（调用方 module_adapters.cpp:8149 对 != OK 一律 fail），影响限于错误分类码（DATA vs UNSUPPORTED 语义）与对外错误契约。
- 修复面：p3_resample.cpp:218 的拒绝集合补 "flux"/"flux_per_pixel"（或统一为「非 surface_brightness 一律 UNSUPPORTED」），并同步 PHASE3_RSMP_IMPL.md §6.2 的 token 清单。只登记不改。（只登记，不改）

### P3R-10 [OTHER] PHASE3_RSMP_IMPL.md 全面过期：239 行 vs 519、签名表为旧签名、行锚全错
- 规范：`docs/algorithms/PHASE3_RSMP_IMPL.md:13-14`「生产源: lib/algorithms/resample/p3_resample.h（58 行，唯一权威签名头）+ lib/algorithms/resample/p3_resample.cpp（**239 行**），实测 2026-09-12」
- 实现：`lib/algorithms/resample/p3_resample.cpp（实测 519 行）、p3_resample.h（实测 150 行）`
- 判定：不符
- 证据：`wc -l` = 519 / 150（文档 239 / 58）。§4 符号表冻结 10 个公共符号并给出旧签名，例如 `p3_sample_nearest | h:46-47 | cpp:232-239 | (const P3Sampler*, const P3WcsDescriptor*, int x, int y, float* value, float* coverage)`；实测签名为 `p3_sample_nearest(P3Sampler*, double ra_deg, double dec_deg, float*, int*)`（p3_resample.h:96-98），且新增 `_ex` 变体（weights[4]/leaf_ipix[4]）、`p3_sampler_attach_cache`、`p3_sampler_cache_stats`、`p3_sampler_set_absent_cache`、`p3_uncertainty_open/propagate/close`。§3 内部结构锚（:18/:22-36/:49-80/:196-230/:232-239）与实测（:43/:59-117/:150-197/:315-406/:294-306）全不对应。
- 影响：以本合同为基线的审计/整改无法定位符号（本次审计实际受阻）；合同对「唯一权威签名头」的冻结已失效。
- 修复面：按实测重刷 PHASE3_RSMP_IMPL.md §3/§4/§6 的行锚与签名表（规范侧文档缺陷，另一类）。只登记不改。（只登记，不改）

### P3R-11 [OTHER] 文档登记 tile cache 为 FIFO，实现已是 LRU（DISP-P3RSMP-002 结论过期）
- 规范：`docs/algorithms/PHASE3_RSMP_IMPL.md:215-220`「有界 FIFO 缓存: 容量默认 8（`set_max_tiles` 可调，≤0 恢复 8）；插入超容时逐出 `keys` 队首（**最旧插入**，cpp:33 `keys.erase(keys.begin())`）——ALG-P3-003 §3 伪代码写 "LRU"，实现为 FIFO（无访问序更新），DISP-P3RSMP-002 如实登记」
- 实现：`lib/algorithms/resample/p3_resample.cpp:74-100`
- 判定：不符
- 证据：`get()` 内 `lru.splice(lru.begin(), lru, it->second.it);`（:78）在每次命中时把条目移到队首 = **真 LRU**（访问序更新）；`put()` 亦 splice（:88）。文件头 :15 自述「跨 worker 共享一个**有界 LRU** tile 缓存」。P30 修复注记（:5-21）说明该层已重写。文档据以登记的 `keys.erase(begin())` FIFO 实现在现文件中不存在（grep `keys.erase` 零命中）。
- 影响：文档对缓存语义的冻结描述与实现相反；DISP-P3RSMP-002 的「实现为 FIFO」结论已过期（命中率/内存行为描述随之失效，但输出像素值不受影响——实现自述逐位等价）。
- 修复面：修订 PHASE3_RSMP_IMPL.md §7 与 PHASE3_RESAMPLE.md:72 的逐出语义为 LRU（并关闭 DISP-P3RSMP-002）。只登记不改。（只登记，不改）

### P3R-12 [OTHER] DISP-P3RSMP-003（check_mode 无调用点）已闭环但文档仍登记为未接线
- 规范：`docs/algorithms/PHASE3_RSMP_IMPL.md:277`「| DISP-P3RSMP-003 | `p3_resample_check_mode` 会话编排层无调用点（flux/variance 输入拒未经会话守卫；能力在内核、探针消费） | grep 全仓: 仅 p3_resample_probe_main.cpp:31 | SCI §9a-8/10 | P3-RSMP-INT 接线（会话请求守卫增加 input_mode 检查） |」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:8140-8153`
- 判定：不符
- 证据：生产节点 op 已调用该守卫：`const astrocs::phase3::P3ResampleStatus mst = astrocs::phase3::p3_resample_check_mode(omode.c_str()); if (mst != P3_RS_OK) return fail(...)`（:8147-8152），且缺 `output_mode` 即拒（:8140-8144）。grep 全仓现命中 lib/infrastructure/scheduler/src/module_adapters.cpp:8148 + p3_resample_probe_main.cpp:31。
- 影响：DISP-P3RSMP-003 已闭环，文档仍登记为未接线（低影响，但会误导后续审计认为该守卫未生效）。
- 修复面：关闭 DISP-P3RSMP-003 并更新 §11 表。只登记不改。（只登记，不改）

## D. module_adapters p3_op_* 与 Phase3 产品合同

### P3X-01 [—] p3_op_* 五节点各自唯一真实入口，未重复调用完整 session
- 规范：`lib/infrastructure/scheduler/src/module_adapters.cpp:7783-7794`「五节点链 source→properties→wcs→resample→writer→verify; 每节点唯一真实 operation 委托（宪章 §7.2 "投影规划和重采样是独立算法节点, 不得重复调用完整 phase3_session_run()" + §8.2 每节点唯一 entrypoint/call count）」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:8034-8699`
- 判定：符合
- 证据：properties→p3_sampler_open_ex+p3_uncertainty_open（:8043-8063）；wcs→p3_wcs_make+p3_wcs_fits_keywords（:8099-8123）；resample→p3_order_select+p3_sample_*_ex+p3_uncertainty_propagate（:8229,8202-8218）；writer→p3_output_write_atomic_ex（:8546）；verify→p3_output_verify_ex（:8647）。各 op 均无 p3_session_run 调用（grep 零命中）。
- 影响：节点粒度与调用计数符合宪章 §7.2/§8.2。
- 修复面：无（符合）（只登记，不改）

### P3X-02 [—] max_tiles 默认 min(1024, ceil(W·H/512²)+16) 且可降不可升
- 规范：`docs/algorithms/PHASE3_FITS_IMPL.md:322`「max_tiles 默认 min(1024, ceil(W·H/512²)+16)，请求可降不可升（:179-193）」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:8208-8226`
- 判定：符合
- 证据：`:8212 need = (wh+per_tile-1)/per_tile + 16; :8213 default_max = min(1024, max(8, need)); :8214 mt = doc.value("max_tiles", default_max); :8215-8220 mt>default_max → RESOURCE fail（可降不可升）; :8221/:8225 signal 与 uncertainty 两个 sampler 均设容量。
- 影响：内存守卫与规范一致，且 P30 修复后容量作用于共享缓存（与 worker 数无关）。
- 修复面：无（符合）（只登记，不改）

### P3X-03 [—] uncertainty 输入选择 variance→ivar 且 provenance 与实测防漂移
- 规范：`docs/contracts/DATA_SEMANTICS.md:2519-2520`「两者并存 → variance 优先，provenance 记 uncertainty_source=variance（一致性数值校验为验证建议，不冻结容差）」
- 实现：`lib/algorithms/resample/p3_resample.cpp:422-461 与 lib/infrastructure/scheduler/src/module_adapters.cpp:8198-8206`
- 判定：符合
- 证据：候选序 `{variance, ivar}` 严格优先（p3_resample.cpp:424-427），先命中即返回；op 侧比对 props artifact 与实时扫描 `props_src != live_src` → DATA fail（:8198-8206），防上下游漂移。
- 影响：输入选择与 provenance 记录一致。
- 修复面：无（符合）（只登记，不改）

### P3X-04 [—] 行带级取消/行带丢弃 fail-closed，不落半成品
- 规范：`docs/algorithms/PHASE3_RESAMPLE.md:106`「取消点: 输出行带粒度(ALG-P3-003 循环)；取消时**输出文件不落盘**(tmp 删除, rename 不发生)」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:8268-8387`
- 判定：符合
- 证据：:8273 每行 `if (ctx && ctx->cancelled()) { cancelled.store(true); break; }`；:8378-8381 取消 → ErrorDomain::CANCELLED 且在 bin/json 写出（:8412/:8463）之前返回；:8382-8387 行带被丢弃（executed≠nw）亦 fail-closed。
- 影响：取消/故障注入不留半成品产物。
- 修复面：无（符合）（只登记，不改）

### P3X-05 [C3] FZ-P3-MODES 三输出模式中 point_source_flux/visualization 在生产被拒
- 规范：`docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json:766`「"id": "FZ-P3-MODES" … "subject": "Phase3 输出模式", "value": "surface_brightness | point_source_flux | visualization", "scope": "Phase3 配置", "gate": "模式未声明即拒绝", "fail_closed": "Phase3 模式未声明或不属 {surface_brightness, point_source_flux, visualization} → REJECT", "status": "FROZEN"」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:8140-8153 + lib/algorithms/resample/p3_resample.cpp:212-221`
- 判定：未实现
- 证据：生产 resample 节点对 output_mode 只放行 `surface_brightness`：p3_resample_check_mode 对 `point_source_flux`/`visualization` 返回 PARAM（:218-220 只特判 weight/flux-per-pixel 与 surface_brightness），节点随即 DATA fail（:8149-8152，注释自述「resample 仅实现 surface_brightness」）。三模式的实现体在 p3_v6_export.cpp:442 `export_product(ExportMode …)` + p3_rsmp_units.cpp:43-50 `parse_mode`（三模式齐备），但无生产调用者（见 P3X-06）。 另证：同一生产 CLI 的模式路由门 lib/infrastructure/cli/v6_runtime_contract.h:130-138 把三模式全部判为 kProduction（rc=0 放行，surface=phase3_export），即 --export-mode point_source_flux 先被 CLI 放行、随后被 p3_op_resample 以 DATA 拒绝——同一条生产路径上前门与后门结论相反。
- 影响：FROZEN 的 Phase3 三输出模式中，point_source_flux（含 Q/W 输出帧重算、EFFECTIVE_PSF/FLUX HDU）与 visualization 在生产 CLI 上不可达：任何 schema/合同合法的 PSF/显示模式请求都被拒。 且 CLI 模式门（v6_runtime_contract.h:134-137）已把二者标为 production 放行，用户会先看到 route_kind=production 的信息事件再吃到 DATA 失败。
- 修复面：接线 p3_v6_export 的三模式导出到生产节点（或把 p3_op_resample 的模式守卫扩到三模式并接入对应传播/写出面）。只登记不改。（只登记，不改）

### P3X-06 [C6] 整个 V6 三模式导出链未编入生产 target，生产执行清单却标 production=yes
- 规范：`docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv:335`「io_writer,p3_v6_export.cpp,lib/phase3_session/p3_v6_export.cpp,production,**yes**,Phase1/2/3,…」
- 实现：`CMakeLists.txt:718-719、lib/algorithms/projection/CMakeLists.txt:13-15`
- 判定：不符
- 证据：`add_library(astrocs_phase3_session STATIC lib/phase3_session/p3_session.cpp)` —— p3_v6_export.cpp **不在任何生产 target 的源列表**（仅 tests/integration/v6_p3/CMakeLists.txt 编译）；p3_proj_v6.cpp 同样无 target（projection/CMakeLists.txt:13「p3_projection.cpp / p3_proj_v6.cpp 的 target 声明归 P3-002 线（本批次未动）」）。全仓 grep `export_product`：仅 p3_v6_export.{h,cpp} 自身 + tests/integration/v6_p3/p3_v6_export_e2e_test.cpp:393/418/448/548。全仓 grep `p3rsmp::`（排除 p3_v6_export 自身）：零命中。 另证：docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv 同文件 :325-326 把 aio/v6 两文件标为 production=yes（其 target 确由根 CMakeLists.txt:886 add_subdirectory(lib/infrastructure/aio/v6) 引入），说明该清单本可按 target 成员资格判定，:335 的 p3_v6_export.cpp 判定与其矛盾。
- 影响：整个 V6 Phase3 链（p3_v6_export.cpp 801 行 + p3_rsmp_*.cpp 6 文件 + p3_proj_v6.cpp 732 行 + aio/v6 的 FITS/单位/provenance 面）在发布二进制中不可达：FZ-P3-MODES/FZ-P3-QW-RECOMPUTE/FZ-P3-BUNIT-QUADRATIC/FZ-P3-KERNEL-REGISTRY 四条 FROZEN 合同在生产运行面上无载体；而生产执行清单把它标为 production=yes，审计据此会得出相反结论。
- 修复面：二选一并登记裁决：(a) 把 p3_v6_export.cpp 与 p3_proj_v6.cpp 编入生产 target 并接线到 CLI export 路径；或 (b) 更正 PRODUCTION_EXECUTION_INVENTORY.csv:335 为 production=no（未接线），并把该链登记为未接线能力。只登记不改。（只登记，不改）

### P3X-07 [C4] V6 导出默认采样核为 bilinear_4quad（冻结表标非生产默认）
- 规范：`docs/algorithms/v6/phase3/ALG-P3-001_KERNEL_REGISTRY.md:24-25`「| `bilinear_4quad` | `registered_with_oracle` | 连续场 | … | 是（`kernel_oracle.json` ok=true, independent=true） | **否** | / | `bilinear_area_overlap_exact` | … | 是 |」
- 实现：`lib/phase3_session/p3_v6_export.h:126`
- 判定：不符
- 证据：`std::string kernel_id = "bilinear_4quad";` —— V6 导出输入结构的**默认核**恰是冻结表标 production_science_default=false 的核；registry 侧实现正确（p3_rsmp_kernel_registry.cpp:198 bilinear_4quad production_science_default=false；:226 bilinear_area_overlap_exact=true）。p3_v6_export.cpp:468-477 用 `in.kernel_id` 经 `KernelRegistry::frozen().admit(kernel, ContinuousField, mode)` 准入——该门只校验注册结构与 allowed_uses，**不校验 production_science_default**（p3_rsmp_kernel_registry.cpp:101-139 无该项判定）。
- 影响：调用方不显式指定核时，连续场科学产品默认用非生产默认核（误差界更宽、语义为「四象限最近中心双线性」而非精确面积重叠），且门禁不会拦截。
- 修复面：p3_v6_export.h:126 默认改为 "bilinear_area_overlap_exact"；并在 KernelRegistry::admit 增加「ContinuousField ∧ !production_science_default ∧ 非显式选择 → Reject」。只登记不改。（只登记，不改）

### P3X-08 [C1] signal 面亮度单位两条链冲突：ADU vs ADU/px²
- 规范：`docs/science/PHASE3_HIPS_TO_FITS.md:50`「`S`: ADU（面亮度语义，GLOSSARY `signal/surface_brightness`；tile 值=每像素面亮度，**非积分通量**）」
- 实现：`lib/algorithms/resample/p3_rsmp_units.cpp:77-80 与 lib/phase3_session/p3_v6_export.cpp:235,503,587`
- 判定：不符
- 证据：V6 线冻结单位表 `const Bunit signal_sb{1,-2}; // ADU/px^2`、`sb_variance_out{2,-4} // ADU^2/px^4`、`sb_ivar_out{-2,4} // px^4/ADU^2`（同 lib/infrastructure/aio/v6/src/v6_bunit.cpp:145-148），导出层写 `BUNIT="ADU/px^2"`（p3_v6_export.cpp:503/:587）与 `units.bunit="ADU/px^2"`（:235）。而 SCI-P3 §9a-11/§3 与 DATA_SEMANTICS §27 规定 signal BUNIT 缺省 'ADU'，生产 alpha writer 实际写 BUNIT='ADU'（module_adapters.cpp:8549 + p3_output.cpp:215-216），VARIANCE/IVAR 写 'ADU^2'/'1/(ADU^2)'（p3_output.cpp:283-284）。p3_v6_export.cpp:18 自注该交叉张力「登记为 finding」。
- 影响：同一物理量（HiPS tile 面亮度）在两条自称生产的链上带不同 BUNIT（ADU vs ADU/px²），相差一个 px² 因子：跨链产品拼接/比较时单位不可直接对齐，任何按 BUNIT 做量纲校验的下游会把其中一条判为错。
- 修复面：先由规范侧裁决 signal 面亮度的权威单位表达（SCI-P3 §3/§9a-11 的 'ADU' vs FZ 单位表的 'ADU/px^2'），再统一 p3_rsmp_units.cpp:77、p3_v6_export.cpp:235/503/587 与 p3_output.cpp:216。只登记不改。（只登记，不改）

### P3X-09 [C7] export 配置合同（嵌套 wcs.center_deg/s_out_deg）与生产平铺键完全不兼容
- 规范：`contracts/schemas/phase_config_export.schema.json`「"required": ["phase_name","config","inputs"], "additionalProperties": false；export_config required = ["output_dir","precision","output_mode","wcs"]；export_wcs required = ["projection","center_deg","s_out_deg","width_px","height_px"]」
- 实现：`lib/infrastructure/cli/parser.cpp:308-311 与 lib/infrastructure/scheduler/src/module_adapters.cpp:7925-7961`
- 判定：不符
- 证据：合同 schema（AGENTS §6 称 contracts/ 为唯一事实源）用嵌套 `config.{output_dir,precision,output_mode,wcs{projection,center_deg[],s_out_deg,width_px,height_px}}` + `inputs`；生产读取面是**顶层平铺** `source.hips_dir / center.{ra_deg,dec_deg} / scale_deg_per_px / width_px / height_px / projection / sampler / longitude_parity / bitpix / coverage_output / max_tiles / frame / output_dir`（p3n_geom :7927-7949；parser 白名单 :309-311）。随附模板 config/templates/export.phase_config.json 按 schema 写（config.wcs.center_deg/s_out_deg）⇒ 键名生产零命中（ci/ledgers/dead_config_keys.json:7-27 已登记 dead_config_key:wcs.center_deg / wcs.s_out_deg，exit_condition「修正模板与 schema 的键名，使生产源码可读」**未闭环**）；且模板 `inputs` 为数组而 parser 要求 `inputs` 为 object（parser.cpp:352-354）⇒ 模板整体不可用。  ‖ [partial-cli-02 补充] 合同（同文件 :26-30）required=["phase_name","config","inputs"] 且 additionalProperties:false；config 内层 required=["output_dir","precision","output_mode","wcs"]（:110-115）。实现模板（session_commands.h:146-164 + config_template :175-186）输出 {"schema_version","source":{"hips_dir"},"output_dir","center":{"ra_deg","dec_deg"},"width_px","height_px","scale_deg_per_px"}。实跑 ./build/astrocs export --template 得上述扁平对象。反向：把合同模板 config/templates/export.phase_config.json 形状的配置喂给 CLI → rc=3 "astrocs: config has unknown key 'config'"（实测）。docs/contracts/CONFIG_CONTRACT.md:24 亦声明 phase_config 族「用户 / `cli --template`」且模板落 config/templates/*.phase_config.json（该三文件存在且为 {phase_name,config,inputs} 形状）。 | 影响: 合同声明的唯一用户写入面（phase_config）在 CLI 上不可达：合同形状配置一律 exit 3，CLI 模板产物又不被合同接受；配置类合同（CFG-001）与生产入口完全脱钩。  ‖ [partial-cli-23 补充] 合同模板 config/templates/export.phase_config.json:8-15 为 {"projection":"tan","center_deg":[0,0],"s_out_deg":0.001,"width_px":512,"height_px":512}；CLI 模板（session_commands.h:151-155）为 {"center":{"ra_deg":0.0,"dec_deg":0.0},"width_px":1024,"height_px":1024,"scale_deg_per_px":0.001}。字段名（center_deg vs center.ra_deg/dec_deg；s_out_deg vs scale_deg_per_px）与尺寸默认（512 vs 1024）双不符。实测 export --template 输出 1024/1024 与 center 对象。 | 影响: 同一导出参数两套命名/默认：按合同模板写的配置在 CLI 上不可解析（contracts/schemas/phase_config_export.schema.json:110-115 要求 wcs 对象），按 CLI 模板写的配置又不满足合同；默认输出尺寸差 4 倍（像素数与磁盘/时间预估随之差 4 倍）。
- 影响：按合同 schema 编写的 export 配置在生产上要么被 parser 拒（unknown key / inputs 形态），要么键被静默忽略（center_deg/s_out_deg）——即「合同即文档」的配置无法跑通；用户唯一可行路径是照 CLI 内部平铺键写，而该形态无合同载体。
- 修复面：裁决唯一配置面：或把 schema/模板改为生产平铺键（source/center/scale_deg_per_px…），或在 p3n_geom/parser 增加 schema 形态到平铺形态的显式映射（含 inputs→source.hips_dir）；同时把 precision/rotation_deg/crpix_px 的消费或拒绝显式化。只登记不改。（只登记，不改）

### P3X-10 [C7] 合同 projection enum 为小写（tan…），生产只接受大写 TAN
- 规范：`contracts/schemas/phase_config_export.schema.json`「"projection": {"enum": ["tan","sin","car","ait","stg","mol","cea","zea"], "description": "投影选择，缺省 TAN"}」
- 实现：`lib/algorithms/projection/p3_wcs.cpp:61-69`
- 判定：不符
- 证据：合同 enum 为**小写**投影码（且含 8 投影）；生产校验 `const std::string proj = projection ? std::string(projection) : std::string("TAN"); if (proj != "TAN") return P3_WCS_UNSUPPORTED;` —— 精确匹配大写 "TAN"，小写 "tan" 被拒（无大小写归一）。CLI 帮助亦写「缺省 TAN」（session_commands.h:156），模板则写 "tan"（config/templates/export.phase_config.json）。
- 影响：schema 合法值 "tan"（合同缺省写法）在生产被显式拒绝；大小写口径不统一使「合同合法即生产可用」不成立。
- 修复面：p3_wcs_validate_request 增加大小写归一（或规范侧统一为 "TAN" 并同步 schema enum）。只登记不改。（只登记，不改）

### P3X-11 [C3] provenance.missing_tiles 恒 nullptr（DISP-P3RSMP-004 未闭环）
- 规范：`docs/science/PHASE3_HIPS_TO_FITS.md:146`「9. **NaN/missing/coverage/mask/alpha channel/blank**：§5/§8——coverage 二值 mask；NaN 传播；**missing tile=无覆盖+provenance**」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:8531`
- 判定：未实现（已登记 DISP-P3RSMP-004）
- 证据：`prov.missing_tiles = nullptr;`（:8531）且 `prov.missing_count = 0`（:8532）；P3Provenance 无 missing tile 聚合面被填充。缺 tile 仅在 resample 节点内部体现为 coverage=0（p3_resample.cpp:378-380），无 tile 标识列表上报。PHASE3_RSMP_IMPL.md:278 已登记 DISP-P3RSMP-004（「provenance.missing_tiles 恒 nullptr（缺 tile 聚合上报未接线）」）。
- 影响：SCI-P3 §9a-9/§8 要求 provenance 记录 missing tile；现产物只能从 coverage=0 反推「有缺失」，无法回答缺了哪些 tile（survey 覆盖审计与可复现性受限）。
- 修复面：（已登记，未闭环）在 p3_op_resample 行带循环内聚合缺失 tile ipix（fetch_tile 的 absent 路径已有计数），经 p3_resampled.json 传入 p3_op_writer 填 prov.missing_tiles/missing_count。只登记不改。（只登记，不改）

### P3X-12 [OTHER] 生产执行清单把未编入二进制的 p3_v6_export.cpp 标为 production=yes
- 规范：`docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv:335`「io_writer,p3_v6_export.cpp,lib/phase3_session/p3_v6_export.cpp,production,yes,Phase1/2/3,aio 原子写(tmp+rename)契约见 IO_AND_ATOMICITY.md,」
- 实现：`CMakeLists.txt:718-728（astrocs_phase3_session 源列表）`
- 判定：不符
- 证据：清单声明 production_reachable=yes 且 phase=Phase1/2/3；实际该 .cpp 不在任何 add_library/add_executable 源列表中（同 P3X-06 证据）。清单同文件其它行（如 :325-326 aio/v6 两文件）确为生产可达，说明该列本应是可判定的。
- 影响：以生产执行清单为审计输入的分片会把未编入二进制的能力当作在役能力（本轮 CONFORM-SWEEP 即以此为入口之一）。
- 修复面：更正 :335 的 classification/production_reachable，或按 P3X-06 完成接线。只登记不改。（只登记，不改）

### P3X-13 [C1] FZ-FORMULA-COV-PROP「唯一方差来源 C_y=R C_x Rᵀ」与生产 Σc_k²u_k 口径冲突
- 规范：`docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json:798`「"id": "FZ-FORMULA-COV-PROP" … 唯一方差来源 `C_y=R C_x R^T`；无任何权重标量反推路径（lib/algorithms/resample/V6_PHASE3_RSMP_IMPL.md:28）」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:8300 + lib/algorithms/resample/p3_resample.cpp:508`
- 判定：规范歧义（已由 UNC 文档登记为 alpha 口径）
- 证据：生产 alpha 路径的 var_out 完全来自权重标量式 `acc += w*w*u_k`（p3_resample.cpp:508，经 p3_uncertainty_propagate 在 op :8300 调用），**不建 C_in、不调 propagate_covariance**（p3_rsmp_covariance.cpp:22-48 的 `C_y = R C_x Rᵀ` 零生产调用者，见 P3X-06）。docs/science/UNCERTAINTY_AND_COVARIANCE.md:100-102 明写「实现口径（如实）：p3_rsmp_covariance.cpp/p3_rsmp_propagation.cpp 现按对角特例 Σc_k²u_k 传播（不建完整 C_in）；该口径是**下界**」；DATA_SEMANTICS §30.4:2542-2546 亦把 Σc_k²u_k 限定为对角特例并警告「通用式必须带协方差项：只写 Σc_k² 会系统性低估（mean|ρ|≈0.19 ⇒ 方差低估 36.3%、σ 低估 20.2%）」。两条权威一为「唯一来源=完整式」、一为「如实登记下界」，对生产可接受性表述相反。
- 影响：生产 Phase3 VARIANCE/IVAR 在相关噪声下系统性低估（文档自报方差 −36.3%/σ −20.2%）；是否可接受取决于使用面（文档要求用该 variance 做测量误差时必须显式加协方差项）。
- 修复面：由负责人裁决：或在生产链路接入完整 C_y=R C_x Rᵀ（需先闭合 CF-T-P3-CORR-EPSILON 的近似相关核数值），或在 FZ/FROZEN 层显式把 alpha 对角口径登记为可接受的受限模式（并让 manifest 携带「covariance_representation=diagonal_lower_bound」标记）。只登记不改。（只登记，不改）

## E. aio（基建 I/O 域，子扫）

### partial-aio-001 [C1] HiPS 读侧 leaf nside 硬编码 2^(order+9)，对 hips_tile_width≠512 静默按 512 处理
- 规范：`docs/science/PHASE3_HIPS_TO_FITS.md:68`「`W≠512`（含 256/1024）→ **显式拒绝**（不静默按 512 处理），扩宽须补跨 tile」
- 实现：`lib/infrastructure/aio/io/hips_core.c:255`
- 判定：不符
- 证据：实现 `h->nside = 1ULL << ((uint64_t)order + 9u);`（+9 恒等于 W=512）；同文件 :221-222 却接受任意 2 的幂 tile 宽 `tw > ACS_HIPS_TILE_WIDTH_MAX || !hips_is_pow2((uint64_t)tw)`（上界 16384，lib/infrastructure/aio/io/include/astrocs/io/hips_input_v1.h:33 `#define ACS_HIPS_TILE_WIDTH_MAX 16384`），:646 按 tile_width 校验 NAXIS、:692-699 按 tile_width²−1 校验 LASTPIX —— 布局按 W 校验、leaf 分辨率按 512 计算。一般式见 docs/science/PHASE3_HIPS_TO_FITS.md:66（`leaf_order = order_sel + log2(W)`）与 :80。对照 C++ 读侧 lib/infrastructure/aio/src/hips/aio_hips_reader.cpp:77-78 `return tile_width == 512 && hips_order >= 0 && hips_order <= 29;`（显式拒绝 W≠512，符合）。
- 影响：W=1024 的输入：若产品头 NSIDE=2^(K+10)（正确），:676-683 的 NSIDE 校验按 2^(K+9) 判定 → 误报 TILE_INVALID；若头恰好 2^(K+9)，则 tile 内 leaf 索引以 18 位（应为 20 位）解释 → ipix→(RA,Dec) 采样位置全错、coverage 与 tile 定位错。
- 修复面：lib/infrastructure/aio/io/hips_core.c:255 改为 `h->nside = (uint64_t)tw << (uint64_t)order;`，并在 hips_validate_properties 内对 tw != 512 返回 ACS_HIPS_ERR_UNSUPPORTED（对齐 SCI-P3 §4 收窄）；或把 io/include/astrocs/io/hips_input_v1.h:33 上界收窄为 512 并同步 IO_002 合同。（只登记，不改）

### partial-aio-002 [C1] properties 写出 hips_frame="equatorial"，两份权威规范文本写 "icrs"
- 规范：`docs/algorithms/HIPS_WRITER.md:196`「hips_tile_width="512" / hips_frame="icrs"（IVOA REC-HIPS-1.0 §4.4.1 值域；」
- 实现：`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1132`
- 判定：不符
- 证据：实现 `kv.push_back({"hips_frame", "equatorial"});`；规范第二处 docs/science/PHASE3_HIPS_TO_FITS.md:63 同样写 `hips_frame`='icrs'（… M1a-B-005 已把写出侧非标准值 "equatorial" 废止 …）。但同文件 :189 自承「本文件 §4 现行文本写“值域 {icrs, galactic, ecliptic} 且 equatorial 已废止”，方向与标准原文相反，已在 run/RELEASE-01/science/SCI-S2-topics.md §3-7 登记，待变更 claim 处理」；实现注释 aio_hips_writer.cpp:1123-1131 与读侧 lib/infrastructure/aio/io/hips_core.c:239-246（equatorial 与 icrs 双收）取同一立场。→ 规范↔实现不符 + 规范之间打架（HIPS_WRITER:196 / SCI-P3:63 ↔ SCI-P3:189）。  ‖ [AIOX-01 补充] 实现：`kv.push_back({"hips_frame", "equatorial"});`（:1132），并在 :1123-1131 注释中给出三方取证（IVOA PR-HIPS-1.0-20170406 §4.4.1 / Hipsgen / CDS Aladin Lite API，2026-09-18）与「'icrs' 不是标准取值」。docs/algorithms/HIPS_WRITER.md:196 亦写 `hips_frame="icrs"`。而同一份 SCI-P3 在 §14b:189 明确记载：「**§4.4.1 properties 键值（本轮核验）**：… **hips_frame 标准值域 = {equatorial, galactic, ecliptic}**（真实 CDS 产品 DSS/2DSSColor、2MASS 实测 hips_frame=equatorial；本轮 web_fetch 逐字核验 2026-09-17）。**注**：本文件 §4 现行文本写"值域 {icrs, galactic, ecliptic} 且 equatorial 已废止"，方向与标准原文相反，已在 run/RELEASE-01/science/SCI-S2-topics.md §3-7 登记，待变更 claim 处理」。读侧兼容已做：lib/algorithms/coverage/hips_properties.cpp:125-126 接受 `equatorial|icrs` 两者，故不构成跨阶段读失败。
- 影响：对只认 icrs 的规范一致性门与按 §4 文本实现的第三方客户端，写出的产品被判非法；自家读写因双收别名不受影响。属于“规范文本方向”争议，科学数值无影响。  ‖ [AIOX-01] 规范与实现方向相反：按 docs/science §4 逐字施工会写出非标准值 'icrs'（真实 HiPS 生态读不到），而现行实现写的是标准值。因读侧双别名兼容，当前无跨阶段故障；风险在于后续按文档「订正」实现会引入非标准产品。
- 修复面：待 SCI/ALG 变更 claim 定稿后二选一：改 lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1132 为 "icrs"，或订正 docs/algorithms/HIPS_WRITER.md:196 与 docs/science/PHASE3_HIPS_TO_FITS.md:63（后者已被 :189 标注待处理）。（只登记，不改）

### partial-aio-003 [C7] v6 phase3 合同把 Phase3 方差传播冻结为对角特例，与 FROZEN 条款及科学权威冲突（同一合同自相矛盾）
- 规范：`contracts/schemas/v6/astrocs.v6.phase3.v1.schema.json:182`「"const": "nearest: var_out = u_in; bilinear: var_out = Sum_k c_k^2 u_k (Sum c_k=1, Sum c_k^2 != 1)",」
- 实现：`docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json:558`
- 判定：不符
- 证据：FROZEN 条款 `"value": "C_out = R C_in R^T; 标量 c^T C_in c"`（:558-563，fail_closed「C_out≠R C_in R^T → REJECT」，scope Phase1/2/3）；docs/contracts/DATA_SEMANTICS.md:2539 主式 `var_out(i) = [ R C_in Rᵀ ]_ii`、:2542 `bilinear:  C_in = diag(u_k) ⇒ var_out = Σ_k c_k² · u_k   # 对角特例（不是通用式）`；docs/science/UNCERTAINTY_AND_COVARIANCE.md:82 主式、:96「成立，禁止当通用式」。而 schema 自身 x-astrocs-production.clause_ids（:310）又列入 "FZ-FORMULA-COV-PROP"。实现现状（科学文档自报）UNCERTAINTY:100-102「p3_rsmp_covariance.cpp/p3_rsmp_propagation.cpp 现按对角特例 Σc_k²u_k 传播」。同一 const 亦在 contracts/proposals/v6/data/astrocs.v6.phase3.v1.schema.json:37。
- 影响：按 schema 校验，任何声明通用式（带协方差项）的产品记录都会被 const 判失败；反向按 schema 通过的产品其 variance 系统性低估（文档自报 mean|ρ|≈0.19 ⇒ 方差低估 36.3%、σ 低估 20.2%），直接污染 Phase3 测量误差。
- 修复面：contracts/schemas/v6/astrocs.v6.phase3.v1.schema.json:182 的 const 改为主式（如 `[R C_in R^T]_ii`，对角式降为例外/note 字段），同步 contracts/proposals/v6/data/astrocs.v6.phase3.v1.schema.json:37；需 SCI/合同变更流程。（只登记，不改）

### partial-aio-004 [C3] 规范指定的 IO-003 原子发布层在链路中无生产调用者，且文件名白名单拒绝 writer 实际产物
- 规范：`docs/algorithms/HIPS_WRITER.md:251`「（runtime/io/hips_output_store.py 临时写→fsync→fitsverify→sha256→原子」
- 实现：`lib/infrastructure/aio/io/hips_output_store.py:606`
- 判定：未实现
- 证据：全仓 grep `HipsOutputStore|publish_directory` 仅命中 tests/io/test_hips_output_contract.py（:38/:83/:101/:144…），lib/、cli/、tools/ 无调用者（兼 C6：有实现零调用）；规范所引路径 `runtime/io/hips_output_store.py` 在现行树不存在（实际文件 lib/infrastructure/aio/io/hips_output_store.py，仅 run/scif2001/shadow/ 有影子副本）。发布器白名单 :595-606 只接受 `properties` / `Moc.fits` / `NorderK/DirD/NpixN.fits`（报错串 :605-606「仅 properties/Moc.fits/NorderK/DirD/NpixN.fits」），而 C++ writer 还产出 metadata.fits（lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1223）、NpixN.tsv（:1367）、metadata.xml（:1444）→ 直接喂 writer 产物必 PublishError。规范要求见 docs/plugins/infrastructure/17_aio.md:21「所有产品：临时文件/目录 + 校验 + fsync + 原子 rename 提交」。
- 影响：规范承诺的原子发布 / COMPLETE manifest / 树哈希在 Phase1 生产链不存在（C++ writer 侧无事务已由 DISP-HIPS-004 登记）；SNR catalogue（.tsv/metadata.xml）与 metadata.fits 无法经该发布层落地 → 发布语义只能靠调用方自律，失败/取消可留半成品目录。
- 修复面：① hips_output_store.py:595-606 白名单增补 metadata.fits / metadata.xml / NorderK/DirD/NpixN.tsv；② 在 Phase1 编排（lib/infrastructure/scheduler/src/module_adapters.cpp 的 p1_op_writer 一带）接线 HipsOutputStore；③ 订正 docs/algorithms/HIPS_WRITER.md:251 的路径锚。（只登记，不改）

### partial-aio-005 [C3] HISS 流式写出 finalize 缺 fsync 步骤（规范要求 flush/close/fsync 后才原子 rename）
- 规范：`docs/plugins/infrastructure/17_aio.md:24`「- 写：分层写（先数据后 manifest）、flush/close/fsync、checksum、原子 rename；」
- 实现：`lib/infrastructure/aio/src/hiss_stream_writer.cpp:647`
- 判定：不符
- 证据：finalize 序列：fflush(temp pool)（:387）→ fclose(temp_pool)（:396）→ 复制子块到 .partial（:569+）→ `std::fclose(fp_out)`（:632，返回值已查）→ `int ret = atomic_replace(pimpl_->partial_path, pimpl_->final_path);`（:647）；全文件 grep 无 fsync / FlushFileBuffers / _commit。对照：同模块 lib/infrastructure/aio/src/aio_atomic_file.h:106-131 是 fflush→fsync→fclose→rename；docs/algorithms/PHASE3_FITS_IMPL.md:93-98（R10-C）正是为同类缺陷补 fsync 并注明「原实现在 close 前 fsync 只能落已入内核页缓存前缀，崩溃可丢数据或留半成品」。
- 影响：崩溃/掉电窗口内 rename 已提交但数据仍在页缓存 → 正式路径出现截断或半成品 .hiss 容器，违反 17_aio.md:21-22（失败/取消不得留下可被误认为正式产品的半成品）与 ASTROCS_DESIGN §9。
- 修复面：lib/infrastructure/aio/src/hiss_stream_writer.cpp:632 之后、:647 之前对 .partial 的 fd 执行 fsync（POSIX）/FlushFileBuffers（Windows），失败即清理 .partial 并返回 HISS_ERR_IO；可直接照 aio_atomic_file.h:112-121 的写法。（只登记，不改）

### partial-aio-006 [C1] AIO 跨边界 ABI 注释把方差分子权重写成 legacy w=a/A_drop，与 DRIZZLE §5 冻结权重不一致
- 规范：`docs/science/DRIZZLE.md:71`「权重一致性: 信号与方差必须用同一 w_jp=a_jp/A_pixel,j（S_p=ΣB_j a_jp/Σa_jp 的」
- 实现：`lib/infrastructure/aio/include/aio_hips.h:119`
- 判定：不符
- 证据：实现注释 `// 方差传播分子 Σ v_j × w_jp² (w_jp = a_jp/A_j_drop)`（:119-120，随 `variance = var_num_sum / covered_area²` 一并冻结给调用方）；DRIZZLE.md:72-74 明确「legacy 权重 a_jp/A_drop,j 使 variance_p 额外乘 1/pixfrac⁴（信号乘 1/pixfrac²），故 SNR 不变但绝对面亮度标度错（DISP-DRZ-009）」。规范侧 HIPS_WRITER:113-119 只写 var_num_sum=Σv_j·w_jp²，未点明 w 的取法。
- 影响：按该 ABI 注释实现或校验的调用方会得到偏大 1/pixfrac⁴ 的 variance/ivar（pixfrac=0.8 ⇒ ×2.44），绝对面亮度标度错；SNR 不受影响，故难以自检。
- 修复面：lib/infrastructure/aio/include/aio_hips.h:119-120 注释改为冻结权重 w_jp=a_jp/A_pixel,j（或显式标注 legacy 与 DISP-DRZ-009 状态）；实际分子由 lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp 传入（不在本分片实现域，未核）。（只登记，不改）

### partial-aio-007 [C4] HISS BUNIT 默认值口径不一：格式头默认 ASTROCS_RELATIVE_FLUX，生产入口强制 ADU
- 规范：`lib/infrastructure/aio/include/hiss_format.h:302`「char     bunit[32] = "ASTROCS_RELATIVE_FLUX";」
- 实现：`lib/infrastructure/aio/src/healpix/aio_healpix_io.cpp:371`
- 判定：规范歧义
- 证据：同模块两处默认冲突：hiss_format.h:302 默认 RELATIVE_FLUX，配合 :105「SIGNAL 已测光校准的累计通量」与 :339「signal[p] = float(sumFlux) — 累计通量（不除面积）」；而 legacy 兼容入口 aio_hiss_write 在 :370-371 置 `hmeta.photappl = 0;` + `std::snprintf(hmeta.bunit, sizeof(hmeta.bunit), "ADU");`。守卫侧 hiss_writer.cpp:321-331 只拒「RELATIVE_FLUX 且 PHOTAPPL=FALSE」，ADU+PHOTAPPL=0 属合法组合，故两态都能落盘。docs/ 与 contracts/ 未给该入口的 BUNIT/photappl 语义条款 → 无法判定哪个是规范默认。
- 影响：若该入口产物被下游当“已定标累计通量”消费，ADU 标签会把相对流量纲读成绝对 ADU（差一个 photscal 因子）；若确为原始 ADU 搬运，则格式默认值 RELATIVE_FLUX 反而误导其他写入方。
- 修复面：在 docs/algorithms/HIPS_WRITER.md（或 hiss_format.h §9/§10 注释）明确 aio_hiss_write(legacy) 的 BUNIT/photappl 语义，并使 hiss_format.h:302 默认与之一致。（只登记，不改）

### partial-aio-008 [C7] k_corr 定义域下界 1 ≤ k_corr 在 v6 合同与实现中均未编码
- 规范：`docs/science/UNCERTAINTY_AND_COVARIANCE.md:49`「- k_corr 定义域 1 ≤ k_corr：k_corr<1 ⇔ N_eff>N_retained（正相关样本的有效样本量不可能」
- 实现：`lib/infrastructure/aio/v6/src/v6_provenance.cpp:279`
- 判定：待定
- 证据：科学域要求 k_corr≥1（:49-50「…`p2_upm_control_variance` 与 `p2_upm_ma_build` 显式拒（rc=1 / rc=7）」）；合同 contracts/schemas/v6/astrocs.v6.provenance.v1.schema.json 的 k_corr.value 仅 `"exclusiveMinimum": 0, "not": {"const": 1}`；实现 v6_provenance.cpp:277-282 只拒 `!(value > 0.0)` 与 `std::fabs(value - 1.0) <= 1e-12`，对 0<k_corr<1 放行。rc=1/rc=7 拒绝面在 lib/phase2（不在本分片实现域，未核）。
- 影响：携带域外 k_corr=0.9 的 provenance 可过 v6 门，UPM control_variance = k_corr·(π/2)·σ_bg²/N_retained 被低估 ≈11%；k_corr<1 物理上不可能（N_eff>N_retained），属域外外推。
- 修复面：contracts/schemas/v6/astrocs.v6.provenance.v1.schema.json 的 k_corr.value 增 `"minimum": 1`（或 exclusiveMinimum:1）；lib/infrastructure/aio/v6/src/v6_provenance.cpp:277 增 `value < 1.0` 拒绝分支并点名 FZ-PROV-KCORR。（只登记，不改）

### partial-aio-009 [C1] properties 行格式：规范冻结节为裸 key=value，v6 渲染器写 key = value
- 规范：`docs/algorithms/HIPS_WRITER.md:192`「- (5b) properties（write_properties :285-293 裸 `key=value\n` 直写，fopen」
- 实现：`lib/infrastructure/aio/v6/src/v6_hips_manifest.cpp:55`
- 判定：规范歧义
- 证据：v6 渲染器 `os << k << " = " << v << "\n";`（:54-56）与 legacy writer 的 `content += '='`（lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:449-450，符合规范 :194）不一致；两个读侧都做 trim 容忍（io/hips_core.c:179-185、v6_hips_manifest.cpp:88-93），故仓内不报错。规范只覆盖 legacy 侧，v6 侧无行格式条款 → 判为歧义。
- 影响：严格按 IVOA「keyword = value」解析的第三方客户端可能读不到 legacy 产物键（反之亦然）；仓内科学结果无影响。
- 修复面：在 docs/algorithms/HIPS_WRITER.md §5b 或 v6 合同补行格式冻结（空格策略），并统一两个 writer 的输出形态。（只登记，不改）

### partial-aio-010 [C4] 同一产品族 hips_status 默认取值在两个生产者间不一致（private master vs public master clonableOnce）
- 规范：`docs/algorithms/HIPS_WRITER.md:200`「传入）/ hips_tile_format="fits" / **hips_status="private master"（恒值」
- 实现：`lib/infrastructure/aio/v6/include/astro/aio/v6_hips_manifest.h:31`
- 判定：规范歧义
- 证据：v6 默认 `std::string hips_status = "public master clonableOnce";`（:31）vs legacy 恒 `kv.push_back({"hips_status", "private master"});`（aio_hips_writer.cpp:1139）。规范只登记 legacy 值并已开 DISP-HIPS-003（无公开/克隆状态参数化），v6 侧无条款。
- 影响：下游若按 hips_status 判可见性/克隆权限，会从同族产品得到相反结论；无科学数值影响。
- 修复面：统一或参数化 aio_hips_writer.cpp:1139 与 v6_hips_manifest.h:31 的取值，并在规范登记取值域与语义。（只登记，不改）

### partial-aio-011 [—] 叶级球面元面积 A_cell = 4π/(12·nside²)
- 规范：`docs/algorithms/HIPS_WRITER.md:45`「- (1b) 单叶球面元面积 `A_cell = 4π / (12·nside²)`（:413），即 Górski 2005」
- 实现：`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:633`
- 判定：符合
- 证据：`ps->A_cell = 4.0 * kPi() / (12.0 * (double)nside * nside);`；层级同式 aio_hips_writer.cpp:1265 `const double A_cell_k = 4.0 * kPi() / (12.0 * (double)nside_k * nside_k);`；HISS 侧同式 lib/infrastructure/aio/src/healpix/aio_healpix_io.cpp:340。
- 影响：support/coverage 的面积归一标度正确（无缺陷）。
- 修复面：无（符合，仅登记分母）。（只登记，不改）

### partial-aio-012 [—] 叶级阶/瓦片阶：leaf_order=ilog2(nside)、tile_order=leaf_order−9、nside≥512、tile_width==512
- 规范：`docs/algorithms/HIPS_WRITER.md:39`「- (1a) 叶级阶 K 与 tile 阶：`leaf_order = ilog2(nside)`（:411），」
- 实现：`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:631`
- 判定：符合
- 证据：`ps->leaf_order = ilog2_u64(nside); ps->tile_order = ps->leaf_order - 9;`（:631-632）；参数门 :616-620（2 的幂 / 512≤nside≤2^29 / tile_width==512 / dtype∈{0,1} / 位域 ≤127）；视图门 :660-663 要求 view->width==512 且 leaf_order/dtype 一致；与 docs/science/PHASE3_HIPS_TO_FITS.md:40（`leaf_order = tile_order + log2(W)`，W=512 ⇒ +9）一致。
- 影响：HIPS 层级与 NSIDE 自洽（无缺陷）。
- 修复面：无（符合）。（只登记，不改）

### partial-aio-013 [—] 叶级 signal=flux/area、support=area/A_cell（钳 1）、无效→NaN 不零填
- 规范：`docs/algorithms/HIPS_WRITER.md:77`「`signal[p] = flux_sum[p] / covered_area[p]`（:477），」
- 实现：`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:711`
- 判定：符合
- 证据：`if (v && area > 0.0 && std::isfinite(flux) && std::isfinite(area)) { sig = flux / area; sup = area / ps->A_cell; area_true = area; if (sup > 1.0) { sup = 1.0; ++ps->support_clamped_pixels; } … } else { sig = std::numeric_limits<double>::quiet_NaN(); }`（:710-723）；层级同构 :1279-1284；与 docs/plugins/infrastructure/17_aio.md:43「缺 tile/非有限 → validity 标记，不以零填充」一致。
- 影响：面亮度语义与 mask 语义正确（无缺陷）。
- 修复面：无（符合）。（只登记，不改）

### partial-aio-014 [—] MOC UNIQ 编码 uniq = 4·4^moc_order + (c >> 2(tile_order−moc_order))
- 规范：`docs/algorithms/HIPS_WRITER.md:187`「`uniq = 4·4^moc_order + (c >> 2·(tile_order−moc_order))`**（:781-785，」
- 实现：`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1247`
- 判定：符合
- 证据：`uniq.push_back(4ULL * (1ULL << (2ULL * ps->moc_order)) + (c >> (2ULL * ((uint64_t)ps->tile_order - ps->moc_order))));`（:1247），随后排序去重 :1248-1249；读侧反解 aio_hips_reader.cpp:252-256 `order_uniq_base = 4ULL * (1ULL << (2ULL * hips_order))`。
- 影响：MOC 单元与叶 tile 一一对应（无缺陷）。
- 修复面：无（符合）。（只登记，不改）

### partial-aio-015 [—] MOC FITS 关键字 ORDERING=NUNIQ / COORDSYS=C / MOCORDER / PIXCOUNT
- 规范：`docs/science/PHASE3_HIPS_TO_FITS.md:190`「- **MOC**：IVOA MOC 1.0/2.0 Recommendation（https://www.ivoa.net/documents/MOC/）§4.3.1：uniq=4×4^order+index（write_moc_fits 的 ORDERING=NUNIQ/MOCORDER 依据）」
- 实现：`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:413`
- 判定：符合
- 证据：`fits_write_key_str(fptr, "ORDERING", (char*)"NUNIQ", …)`（:413）、COORDSYS=C（:414）、MOCORDER（:415）、PIXCOUNT（:416）；UNIQ 列 TFORM="K"（:403-404）；空集不写（:394）。
- 影响：MOC 可被标准客户端读取（无缺陷）。
- 修复面：无（符合）。（只登记，不改）

### partial-aio-016 [—] tile 路径 NorderK/Dir{(ipix/10000)*10000}/Npix{ipix}.fits
- 规范：`docs/algorithms/HIPS_WRITER.md:97`「CHECKSUM 完整性）。路径 `Norder{K}/Dir{(ipix/10000)*10000}/Npix{ipix}.fits`」
- 实现：`lib/infrastructure/aio/io/hips_core.c:570`
- 判定：符合
- 证据：`snprintf(rel, rel_cap, "Norder%d/Dir%llu/Npix%llu.fits", (int)h->order, (unsigned long long)((ipix / 10000u) * 10000u), (unsigned long long)ipix);`（:570-572，注释 :557-558 引 IVOA REC-HIPS-1.0 §4.1）；旧布局仅作读侧回退 :577-584/:585-607；lib/infrastructure/aio/src/hips/aio_hips_reader.cpp:41-43 同式；发布器白名单同形 hips_output_store.py:600-603。
- 影响：目录约定与 IVOA §4.1 一致（无缺陷）。
- 修复面：无（符合）。（只登记，不改）

### partial-aio-017 [—] hips_pixel_scale 公式与单位（度）
- 规范：`docs/algorithms/HIPS_WRITER.md:214`「/ hips_hierarchy / **hips_pixel_scale = (180/π)·√(π/3)/nside deg（IVOA REC-」
- 实现：`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1115`
- 判定：符合
- 证据：`std::snprintf(buf, sizeof(buf), "%.6f", 180.0 / kPi() * std::sqrt(kPi() / 3.0) / (double)ps->nside);`（:1115，注释 :1112-1114 引 IVOA §4.4.1 单位=度）；独立复算 nside=512 ⇒ 0.1145162137 deg（写盘 "0.114516"）。
- 影响：与 IVOA 同名键量纲一致（无缺陷；旧实现多乘 3600 已修）。
- 修复面：无（符合）。（只登记，不改）

### partial-aio-018 [—] 帧尺度上界常量 = 2×最粗叶像素角尺度（824.5167388361774″）
- 规范：`docs/algorithms/HIPS_WRITER.md:212`「412.258369″，经 SCI-DRZ-001 冻结的 1–2× 过采样 ⇒ 帧尺度 ≤2×412.258369″；」
- 实现：`lib/infrastructure/aio/include/aio_hips.h:100`
- 判定：符合
- 证据：`#define ACS_HIPS_MAX_FRAME_SCALE_ARCSEC 824.5167388361774`；独立复算（Python FP64）2·(√(π/3)/512)·(180/π)·3600 = 824.5167388362，与常量差 0.000e+00；推导注释同文件 :90-99；使用点 lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:1484-1502（aio_hips_set_drizzle_provenance 拒域外值，判据 :1502）。
- 影响：帧尺度合法域与冻结推导一致（无缺陷）。
- 修复面：无（符合）。（只登记，不改）

### partial-aio-019 [—] HISS 容器 JSON 头 zstd level=5（.hcsd 路径）；子块 codec 级别属未冻结面
- 规范：`lib/infrastructure/aio/docs/HEALPIX_FORMAT_SPEC.md:38`「- 使用 zstd 压缩，**压缩级别 level=5**」
- 实现：`lib/infrastructure/aio/src/healpix/aio_healpix_io.cpp:45`
- 判定：符合
- 证据：`static const int ZSTD_LEVEL = 5;`（:45）用于 JSON 头压缩 :1209（aio_hcsd_write）与 :1827（aio_hiss_write_snr_model）；子块 codec 默认级别 3（lib/infrastructure/aio/src/hiss_codec.cpp:115 `static const int kZstdDefaultLevel = 3;`）属 hiss_format.h:16-18 声明的未冻结事项 DQ-001~004，两处不冲突（zstd 帧自带级别，解压与级别无关）。
- 影响：无科学影响（容器头可正常解压）。
- 修复面：无（符合）。（只登记，不改）

### partial-aio-020 [—] HISS 子叶分区 nside=64 / 49152 子叶 / 索引项 24B / ipix>>14
- 规范：`lib/infrastructure/aio/docs/HEALPIX_FORMAT_SPEC.md:210`「- ipix 嵌套位运算：`leaf_ipix = ipix_fine >> (2 × log2(8192/64))`，即 `ipix_fine >> 14`」
- 实现：`lib/infrastructure/aio/src/healpix/aio_healpix_io.cpp:261`
- 判定：符合
- 证据：`hio_compute_leaf_shift`：`while (temp > 64) { shift += 2; temp >>= 1; }`（:261-269）⇒ nside=8192 → 14；`static const uint64_t N_LEAVES = 49152;`（:41）、`LEAF_INDEX_SIZE = 49152 * 24`（:42）、`static_assert(sizeof(LeafIndexEntry) == 24, "LeafIndexEntry must be 24 bytes");`（:279）。
- 影响：子叶索引 O(1) 定位正确（无缺陷）。
- 修复面：无（符合）。（只登记，不改）

### partial-aio-021 [—] HISS support uint8 = round(255·clamp(sum_area/A_p,0,1))
- 规范：`lib/infrastructure/aio/include/hiss_format.h:369`「HISS_EXPORT void finalize_support(std::vector<uint8_t>& support) const; // uint8 round(255*S/A_p)」
- 实现：`lib/infrastructure/aio/src/hiss_common.cpp:132`
- 判定：符合
- 证据：`double S = pixels[i].sum_area / A_p;`（:128）→ `if (S < 0.0) S = 0.0; if (S > 1.0) S = 1.0;`（:130-131）→ `long v = std::lround(255.0 * S);`（:132）；validate_support 以 eps=1e-4 判超限（:146-163）、pixel_area<=0 硬失败（:148-153）；与 HIPS_WRITER:79-84 的 covered_area=(support/255)·A_cell 反解自洽。
- 影响：support 面与 Phase1 编排口径自洽（无缺陷）。
- 修复面：无（符合）。（只登记，不改）

### partial-aio-022 [—] 缺 tile/非有限 → validity 标记不零填；缺 tile 不做父阶回退
- 规范：`docs/plugins/infrastructure/17_aio.md:43`「- 缺 tile/非有限 → validity 标记，不以零填充；」
- 实现：`lib/infrastructure/aio/io/hips_core.c:626`
- 判定：符合
- 证据：C 读侧缺 tile → `hips_set_err(err, cap, "tile 缺失: %s (不做父 order 回退)", rel); return ACS_HIPS_ERR_TILE_MISSING;`（:625-628）；dtype 门 :655-658；C++ 读侧尺寸门 aio_hips_reader.cpp:141/:191；writer 无效像素写 NaN（aio_hips_writer.cpp:722）。零值仅作 HISS 容器“无贡献”哨兵（hiss_format.h:104/:364），读取时以 `signal[local] != 0.0f || support[local] > 0` 保留有覆盖像素（aio_healpix_io.cpp:505）→ 与 §7 不冲突。
- 影响：mask/validity 语义正确（无缺陷）。
- 修复面：无（符合）。（只登记，不改）

### partial-aio-023 [—] FITS 读写单位（BUNIT）检查与 BUNIT 卡写出
- 规范：`docs/plugins/infrastructure/17_aio.md:23`「- 读：格式校验、哈希校验、单位/形状/所有权检查；」
- 实现：`lib/infrastructure/aio/io/fits_core.c:847`
- 判定：符合
- 证据：读侧期望单位不符即拒：`set_err(err, cap, "unit mismatch: expect BUNIT='%s' file BUNIT='%s'", …)`（:847）；解析 BUNIT 入 hdr（:446）；写侧 `fio_card_write(f, "BUNIT", bunit, "physical units of the array values");`（:1138）；C++ 读侧缺省 ADU：lib/infrastructure/aio/src/aio_fits.cpp:495-497。
- 影响：单位不符显式 MISMATCH 不静默（无缺陷）。
- 修复面：无（符合）。（只登记，不改）

### partial-aio-024 [—] AIO UPM 稠密缓存 tile_shift=9 / mask=(1<<18)-1（512² 叶块）
- 规范：`docs/algorithms/HEALPIX_MAPPING.md:23`「tile_shift=9；mask=(1<<18)-1；nested_local_to_xy 单调。」
- 实现：`lib/infrastructure/aio/src/aio_upm.cpp:524`
- 判定：符合
- 证据：`const int tile_shift = 9;`（:524）、`const std::uint64_t mask = (1ULL << (2u * (unsigned)tile_shift)) - 1ULL;`（:525）；tile=leaf_ipix>>18、local=leaf_ipix&mask（:530-532）；dense 头 `"leaf_order":%d` 写 target_order+9（:226-229）与 HIPS_WRITER:39-44 同构。
- 影响：UPM 稠密缓存按 512² tile 定位正确（无缺陷）。
- 修复面：无（符合）。（只登记，不改）

### partial-aio-025 [C1] signal 面亮度单位口径打架：SCI-P3 写 ADU，DRIZZLE/冻结单位表写 ADU/px^2；实现侧不写 BUNIT
- 规范：`docs/science/PHASE3_HIPS_TO_FITS.md:50`「- `S`: ADU（面亮度语义，GLOSSARY `signal/surface_brightness`；tile 值=每像素面亮度，**非积分通量**）；`s_out`: deg/px；`center/CRVAL/RA,Dec`: deg（ICRS）；坐标: px；coverage: 无量纲 {0,1}。」
- 实现：`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:339`
- 判定：规范歧义
- 证据：同族权威另一口径：docs/science/DRIZZLE.md:59「【输出 S_p = 面亮度】(ADU/px²)」；冻结条款 docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json:254 `"value": "ADU/px^2"`（FZ-UNIT-SIGNAL-SB，scope Phase1 signal，negative_mutation 明写「把 signal_sb 单位改为 ADU … 量纲代数门必须 rc!=0」）；contracts/schemas/v6/astrocs.v6.units.v1.schema.json 同表。实现侧：HiPS tile 头卡集 write_fits_image（aio_hips_writer.cpp:339-349）无 BUNIT，properties 键集（:1117-1217）亦无 BUNIT/units 键，读侧缺省 ADU（lib/infrastructure/aio/src/aio_fits.cpp:495-497 `else aio_safe_copy(cal.bunit, AIO_BUNIT_MAX, "ADU");`）。
- 影响：按 FZ-UNIT-SIGNAL-SB 的量纲门，缺 units 声明的 signal 产品应 unavailable/REJECT；而实现根本不写 BUNIT，故该门只能在 v6 侧对“记录”生效、对 FITS 产物无约束。单位串分歧本身不改数值，但会使跨阶段单位核对（ADU vs ADU/px²）无法机械判定。
- 修复面：SCI 变更 claim 统一 SCI-P3:50 与 DRIZZLE:59 的单位串；实现侧在 HiPS properties 增 BUNIT（或 v6 units 记录）以让 FZ-UNIT-SIGNAL-SB 可判 —— 归属与流程由负责人裁决。（只登记，不改）

### partial-aio-026 [C1] C 读侧接受 BITPIX=8 的 signal tile，与 SCI-P3「float FITS tile / int tile 显式拒绝」及 C++ 读侧策略冲突
- 规范：`docs/science/PHASE3_HIPS_TO_FITS.md:122`「| JPEG/PNG/int+BLANK/多通道 tile | 显式拒绝（alpha 范围外） |」
- 实现：`lib/infrastructure/aio/io/hips_core.c:655`
- 判定：不符
- 证据：C 读侧 `if (bitpix != ACS_FIO_BITPIX_U8 && bitpix != ACS_FIO_BITPIX_F32 && bitpix != ACS_FIO_BITPIX_F64) { … "tile BITPIX=%d 不支持 (仅 8/-32/-64)" }`（:655-658）→ 8 位整数 tile 被判 PRESENT；其接口注释 lib/infrastructure/aio/io/include/astrocs/io/hips_input_v1.h:129 亦写「tile 实际 BITPIX: -32 原样 / -64 原样 / 8 按字节提升」。规范侧 SCI-P3:63 要求「数据属性含 float FITS tiles」、:122 表列「int+BLANK tile → 显式拒绝」。C++ 读侧 lib/infrastructure/aio/src/hips/aio_hips_reader.cpp:148-166 只收 -32/-64（`set_err("tile BITPIX 非 -32/-64")`），两读侧策略相反。
- 影响：8 位 display-stretch/int tile 被 C 读侧当合法科学平面提升为 f32 → 若下游用它做测光/面亮度，量化误差最大 1/255（约 0.4%）且与 alpha 拒绝面不符；同一产品在两条读路径上判定相反。
- 修复面：lib/infrastructure/aio/io/hips_core.c:655-658 去掉 ACS_FIO_BITPIX_U8 接受分支（返回 ACS_HIPS_ERR_TILE_INVALID），并同步 io/include/astrocs/io/hips_input_v1.h:129 注释与 IO_002 合同；诊断平面 int32 走 C++ read_tile_i32（BITPIX=32）不受影响。（只登记，不改）

## F. cli / hips_browser（基建 CLI 域，子扫）

### partial-cli-01 [C7] --version --json 输出不满足 contracts/schemas/version.schema.json（缺 6 必填字段且含被禁字段）
- 规范：`contracts/schemas/version.schema.json:6`「"required": ["version", "prerelease", "commit", "dirty", "build_id", "abi_version", "cli_schema_version"],」
- 实现：`lib/infrastructure/cli/commands.cpp:1853`
- 判定：不符
- 证据：实现 printf 只输出 3 键：{"schema_version":"1","name":"astrocs","version":"<ver>"}；schema 顶层 additionalProperties:false（version.schema.json:5）故 schema_version/name 亦非法。实跑 ./build/astrocs --version --json → {"schema_version":"1","name":"astrocs","version":"0.11.0-alpha.2+gdb7af37fb96870c167e8eac16cdd3bc7dfb7edf6"} rc=0。旁证 docs/VERSIONING.md:16「--version --json 输出至少：version, prerelease(=alpha), commit, dirty, build_id, abi_version, cli_schema_version」。测试 tests/cli/test_cli_protocol.py:96 名为 test_02_version_json_schema 但只断言 name/schema_version/version 正则，未加载 version.schema.json（假门）。
- 影响：外部消费者按合同解析版本对象（prerelease/commit/dirty/build_id/abi_version/cli_schema_version）全部取不到；版本/ABI/CLI schema 版本无法机器读取，DOCCHK「schema 校验」名存实亡。
- 修复面：lib/infrastructure/cli/commands.cpp:1851-1859 —— 改为输出 version.schema.json 全字段（commit/dirty/build_id/abi_version/cli_schema_version 由 version_generated.h 注入），并把 tests/cli/test_cli_protocol.py:96 换成真 schema 校验。（只登记，不改）

### partial-cli-03 [C7] run manifest 与 contracts/schemas/run_manifest.schema.json 冲突（实现符合 MANIFEST_VERIFY_V1 §2）
- 规范：`contracts/schemas/run_manifest.schema.json:24`「"manifest_schema",」
- 实现：`lib/infrastructure/cli/commands.cpp:227-250`
- 判定：规范歧义
- 证据：合同要求 required=[manifest_schema,run_id,software_sha,config_hash,manifest_input_hashes,manifest_output_hashes,toolchain_version,created_utc] 且 additionalProperties:false（:23-33）。实现写 {"schema_version":"1","kind":"astrocs_run_manifest","run_id","astrocs_version","platform","config_path","config_sha256","cpu_profile_path","cpu_profile_sha256","phases","artifacts","status","started_utc","finished_utc","summary"}，与 docs/api/MANIFEST_VERIFY_V1.md:24-29（FROZEN API-MANIFEST-001）逐键一致。两份规范/合同互斥（字段名族、additionalProperties）。
- 影响：同一产物两套合同：按 run_manifest.schema.json 校验必红，按 MANIFEST_VERIFY_V1 校验必绿；审计/打包侧若引错合同会误判。
- 修复面：负责人裁决单一合同：要么把 contracts/schemas/run_manifest.schema.json 对齐 MANIFEST_VERIFY_V1 §2（或标注 superseded），要么改 CLI 写合同形状并同步 verify/inspect。（只登记，不改）

### partial-cli-04 [C7] JSONL artifact 事件缺 CLI_PROTOCOL §4 的 DET-001 四字段（文档 8 字段 vs schema/实现 4 字段）
- 规范：`docs/api/CLI_PROTOCOL_V1.md:44`「artifact{role,path,sha256,size_bytes,integrity_sha256,canonical_sha256,canonical_hash_spec,canonical_format}（**DET-001**：sha256=整文件字节摘要(完整性)，canonical_sha256=规范产品哈希(像素数据+科学元数据，排除易变卡/键；口径 spec=astrocs.canonical-product-hash/v1，见 tools/canonical_product_hash.py --spec)；可复现性判据用 canonical_sha256，不得用 sha256）」
- 实现：`lib/infrastructure/cli/protocol.h:46`
- 判定：规范歧义
- 证据：实现硬闸只要求 4 字段：{"artifact", {"role", "path", "sha256", "size_bytes"}}（protocol.h:46），与 contracts/schemas/jsonl_event_v1.schema.json:87 "then": { "required": ["role", "path", "sha256", "size_bytes"] } 一致。发射点 commands.cpp:281-285（run manifest artifact）与 :453-455（graph_dir）均不发 integrity_sha256/canonical_sha256/canonical_hash_spec/canonical_format；canonical_* 只出现在 run manifest 的 artifacts[] 行（commands.cpp:129-142）。
- 影响：GUI/外部 harness 按文档从 JSONL artifact 行取可复现性判据（canonical_sha256）会得到 undefined；DET-001 的「不得用 sha256 判可复现」在协议面无强制点。
- 修复面：裁决：若文档为准 → protocol.h:42-49 增补 4 字段并让 commands.cpp:281/453 携带 canonical_*（或删除该 artifact 事件）；若 schema 为准 → 修订 CLI_PROTOCOL_V1.md:44 去掉 DET-001 扩展字段。（只登记，不改）

### partial-cli-05 [C7] module.yaml 的 entrypoint 符号与 data_contracts 路径均不存在
- 规范：`lib/infrastructure/cli/module.yaml:13`「entrypoint: astrocs::cli::cmd::run」
- 实现：`lib/infrastructure/cli/module.yaml:13`
- 判定：不符
- 证据：全仓 grep "cli::cmd::run" 仅命中 module.yaml:13 自身；lib/infrastructure/cli 下不存在自由函数 run（命令入口是 dispatch(commands.cpp:1846)/Subcommand::dispatch(subcommand.h:356)）。同文件 :25 "- contracts/schemas/v6/phase_config.schema.json" 指向不存在的文件（ls contracts/schemas/v6/phase_config.schema.json → No such file；该目录只有 astrocs.v6.{units,signal,weight-mode,point-information,psf,effective-psf,covariance,phase3,psfsw,provenance}.v1.schema.json）。
- 影响：模块元数据合同（ENGINEERING_SPEC §4/11_MODULE_SOURCE_TEST_STANDARD §4）不可机器解析：entrypoint 无法解析、数据合同悬挂；任何按 module.yaml 装配/校验的工具都会失败或静默跳过。
- 修复面：lib/infrastructure/cli/module.yaml:13 改为真实入口（如 astrocs::dispatch / astrocs::cli::cmd::Subcommand::dispatch）；:25 改为实际存在的合同路径（contracts/schemas/jsonl_event_v1.schema.json 等）或删除该行。（只登记，不改）

### partial-cli-06 [C7] 18_cli.md 引用的两个合同文件不存在（规范侧悬挂引用）
- 规范：`docs/plugins/infrastructure/18_cli.md:16`「- 参考：`contracts/schemas/cli_output.schema.json`、`contracts/schemas/events.schema.json`。」
- 实现：`contracts/schemas/（无对应文件）`
- 判定：未实现
- 证据：ls contracts/schemas/cli_output.schema.json / events.schema.json 均不存在；grep cli_output|events\.schema 在 contracts/ 零命中。实际协议合同是 contracts/schemas/jsonl_event_v1.schema.json（18_cli.md:26 提到的 JSONL 事件即它）。
- 影响：插件文档的「输入/输出数据合同」指向空气：读者/检查器按路径取合同必失败，掩盖真实合同（jsonl_event_v1）。
- 修复面：docs/plugins/infrastructure/18_cli.md:16 改为 contracts/schemas/jsonl_event_v1.schema.json（+ 若确需 cli_output 合同则新建并登记）。（只登记，不改）

### partial-cli-07 [C7] 23_hips_browser.md 引用的 hips_product.schema.json 不存在
- 规范：`docs/plugins/infrastructure/23_hips_browser.md:17`「- 参考：`contracts/schemas/hips_product.schema.json`（读侧复用）。」
- 实现：`contracts/schemas/（无对应文件）`
- 判定：未实现
- 证据：grep hips_product 在 contracts/ 零命中；contracts/schemas/ 下无 hips_product.schema.json（HiPS 相关合同只有 contracts/data/examples/frame_hips_manifest.example.json 与 contracts/data/artifact_manifest.schema.json）。
- 影响：浏览器读侧的「只读复用」合同无落点，无法机器校验 HiPS 产品可读性约束。
- 修复面：docs/plugins/infrastructure/23_hips_browser.md:17 指向既有合同（contracts/data/artifact_manifest.schema.json / aio 读侧 schema）或补建 hips_product.schema.json。（只登记，不改）

### partial-cli-08 [C7] cli_selftest / cli_modules_list 两份合同服务于已删除命令面（陈旧合同）
- 规范：`docs/api/CLI_PROTOCOL_V1.md:7`「别名（含 config */modules */selftest/test synthetic/verify*/drizzle/benchmark cpu|」
- 实现：`contracts/config/cli_selftest.schema.json:4`
- 判定：规范歧义
- 证据：CLI_PROTOCOL_V1.md:7-8 明确「…selftest/test synthetic… 全部删除且 rc=2」（实测 tests/cli/test_cli_protocol.py:121 ("selftest","--json") 断言 rc=2），但 contracts/config/cli_selftest.schema.json:4 仍写 "title": "astrocs selftest --json 输出合同 (CLI-001 冻结)"，cli_modules_list.schema.json:4 同（modules list）。CLI 现无 selftest/modules 命令（command_tree.h:79-99 无此条目）。
- 影响：合同集包含无载体的冻结合同：清单/一致性检查会把「有合同无命令」当有效能力，掩盖能力删除（ENGINEERING_SPEC §8 退役纪律）。
- 修复面：按 ENGINEERING_SPEC §8 显式退役这两份合同（标注 RETIRED + 指向 doctor --json 的替代面），或在 CLI_PROTOCOL_V1.md §1 说明其为历史冻结件。（只登记，不改）

### partial-cli-09 [C7] module.yaml 声明 header-only/DLL 名，与实现（5 个 .cpp 编入 astrocs target、无独立 DLL）不符，且硬编码版本
- 规范：`lib/infrastructure/cli/module.yaml:12`「module_status: IMPLEMENTED_HEADER_ONLY」
- 实现：`CMakeLists.txt:769-773`
- 判定：不符
- 证据：根 CMakeLists.txt:769-773 add_executable/源清单含 lib/infrastructure/cli/{main.cpp,parser.cpp,commands.cpp,process.cpp,runtime_client.cpp}（实测 build/ 下存在 libastrocs_cli_process.a / libastrocs_cli_runtime.a），而 module.yaml:11 dll_name: astrocs_infrastructure_cli.dll 无产物、:12 状态写 HEADER_ONLY；:8 module_version: 0.11.0-alpha.2 与根 VERSION（0.11.0-alpha.2）重复硬编码（VERSIONING.md §2「禁止多处手填」）。
- 影响：模块清单元数据与构建事实矛盾：装配/打包工具据 dll_name 找不存在的 DLL；版本字面量在 lib/ 下不受 check_version_consistency（其扫描面为 docs/schemas/tools/launch/tests）保护，漂移不会被门禁抓到。
- 修复面：lib/infrastructure/cli/module.yaml:8/11/12 按实况订正（module_version 由 gen_version 注入、去掉不存在的 dll_name 或标注 CLI 为 exe 内嵌），并纳入 CHK-PACKAGE/版本检查扫描面。（只登记，不改）

### partial-cli-10 [C1] RESOURCE 错误域映射到退出码 5（应为 10）
- 规范：`lib/infrastructure/cli/exit_codes.h:17`「RESOURCE      = 10,  // 资源利用率或内存增长门禁失败」
- 实现：`lib/infrastructure/cli/runtime_client.cpp:419`
- 判定：不符
- 证据：实现 switch：case astrocs::core::ErrorDomain::RESOURCE: return 5;（runtime_client.cpp:419），且 :399 注释自述「RESOURCE → 5」。DESIGN §6.3 表（ASTROCS_DESIGN.md:425）与 CLI_PROTOCOL_V1.md:34 均为「10 资源利用率或内存增长门禁失败」；5 是「backend ABI/签名/CPU 特征/加载失败」。
- 影响：运行期资源域失败被报成 backend 失败：CI/验收按码分流会误判根因（资源门禁失败看起来像 ABI 问题），10 语义在该路径不可达。
- 修复面：lib/infrastructure/cli/runtime_client.cpp:415-421 —— RESOURCE → astrocs::RESOURCE(10)，并改用 exit_codes.h 具名常量替代字面量。（只登记，不改）

### partial-cli-11 [C1] 模块注册/运行时创建失败返回 70（应为 5 backend ABI/加载失败）
- 规范：`docs/plugins/infrastructure/19_runtime.md:80`「- ABI/签名/CPU 特征不匹配 → exit 5；」
- 实现：`lib/infrastructure/cli/runtime_client.cpp:346`
- 判定：不符
- 证据：register_cli_modules(reg) 失败 → return 70;（:344-347）；create_runtime(budget) 失败 → return 70;（:349-352）。而 19_runtime.md:6「模块注册/加载与 ABI 校验由调度器承担」，18_cli.md:48「ABI/加载失败 → 5」。两条路径都返回未分类内部错误码。
- 影响：模块装配/ABI 失败被归类为「未分类内部软件错误」并触发 crash-report 语义，掩盖 backend 面根因；按 04 表分流的消费者拿不到 5。
- 修复面：lib/infrastructure/cli/runtime_client.cpp:346/351 改为 astrocs::BACKEND(5)；若确有非 ABI 的内部失败需 70，按错误域细分而非统一 70。（只登记，不改）

### partial-cli-12 [C1] -force + output_dir 非字符串 → rc=70 崩溃（export），与同文件自述「即使 -force 也必须 rc=2」矛盾
- 规范：`lib/infrastructure/cli/subcommand.h:60`「// 结构错不是 §3.5 的可强制项（-force 只越过「缺校准帧」一类），因此即使 -force」
- 实现：`lib/infrastructure/cli/subcommand.h:278-282`
- 判定：不符
- 证据：实跑：config={"output_dir":123,"source":{"hips_dir":"..."}}；./build/astrocs export --json <cfg> → rc=2（正确）；同配置加 -force → rc=70，stderr "astrocs: CRASH run_id=... detail='[json.exception.type_error.302] type must be string, but is number'"。根因：subcommand.h:278-282 在 config_structure_errors(:292) 之前对 -force 直接早退，而 commands.cpp:1167-1168 `prior_cfg_doc.value("output_dir", std::string("."))` 在 nlohmann 中类型不符即抛 type_error。normalize/mosaic 同输入 rc=2（走 validate_config_full:384-390）。
- 影响：「我知道我在干什么」的总开关把一个可诊断的配置错变成内部崩溃码 70 + crash report；同一输入三命令码不一致（2/2/70），跨平台同失败同码与负例矩阵失效。
- 修复面：commands.cpp:1167-1168（及 :1219/:986/:1233/:1509 等 value(key, std::string(".")) 同族）改为 contains+is_string 判定后再取值；或在 subcommand.h:278-282 的 -force 早退前仍执行 config_structure_errors（恢复注释自述语义）。（只登记，不改）

### partial-cli-13 [C1] 退出码数值字面量散落在 runtime_client.cpp（唯一源纪律被绕过）
- 规范：`docs/plugins/infrastructure/18_cli.md:44`「- 退出码唯一源 `include/astrocs/exit_codes.h`（0/2/3/4/5/6/7/8/9/10/70）。」
- 实现：`lib/infrastructure/cli/runtime_client.cpp:346`
- 判定：不符
- 证据：runtime_client.cpp 直接写 return 70;（:346,:351,:420）、return 2;（:357,:416）、return 4;（:362）、return 3;（:413）、return 7;（:417）、return 9;（:418）、return 5;（:419）。同文件已 include exit_codes.h 且有 astrocs::OK 等具名常量用法，属口径双轨。
- 影响：数值表不再是单点：一处改码（如修正 RESOURCE→10）无法靠类型系统/单源检索发现其他字面量漂移，正是 partial-cli-10 未被测试拦住的机制。
- 修复面：lib/infrastructure/cli/runtime_client.cpp 全部 return <num> 改为 astrocs::<NAME>；CI 增 grep 门禁（cli/** 内不得出现退出码字面量 return）。（只登记，不改）

### partial-cli-14 [C1] IR 静态验证失败映射到 4，而同类 IR 构建失败在 validate 面映射到 2（内部口径分叉）
- 规范：`lib/infrastructure/cli/commands.cpp:1398`「return astrocs::ARGS;   // 与 run 的 IR 构建失败映射一致(runtime_client → 2)」
- 实现：`lib/infrastructure/cli/runtime_client.cpp:362`
- 判定：待定
- 证据：runtime_client.cpp:359-363 load_pipeline 失败 → return 4; 注释「静态验证失败 → 科学/配置错误」；而 commands.cpp:1395-1399 同一类「IR 不可构建」→ ARGS(2) 且注释自称「与 run 的 IR 构建失败映射一致」。规范侧（18_cli.md:48 / DESIGN §6.3）只区分「参数或配置错误=2」「科学验证或不变量失败=4」，未规定 IR/pipeline 静态校验归属 → 判不了哪边对。
- 影响：同一种「配置导致的图不合法」在 validate/plan 面是 2、在 run 面是 4；机器按码分流的验收会得到不一致结论。
- 修复面：请负责人裁决 IR/pipeline 静态校验归属（建议 2 配置错），统一 commands.cpp:1398 与 runtime_client.cpp:362。（只登记，不改）

### partial-cli-15 [C3] --cpu-profile 被解析但零消费：线程预算取自硬件 affinity，profile 从不读取
- 规范：`docs/plugins/infrastructure/19_runtime.md:65`「| `workers` | 由 profile | —— | 线程预算（来自 cpu_profile，不得硬编码） |」
- 实现：`lib/infrastructure/cli/commands.cpp:1656`
- 判定：未实现
- 证据：budget 来源：cmd_session1_run :1656 `const uint32_t budget = cli_affinity_cpu_count();`（phase2 :1012、phase3 :1260 同），cli_affinity_cpu_count 定义 :94-106（sched_getaffinity/GetActiveProcessorCount）——纯硬件核数。--cpu-profile 只登记在 command_tree.h:59/83-89 与 parser.cpp:43 的值旗标表；validate_cpu_profile（parser.cpp:404-429）全仓零调用者（grep validate_cpu_profile 仅命中定义与声明 cli_common.h:95）。run manifest 的 cpu_profile_path/cpu_profile_sha256 恒为 null（commands.cpp:242-243）。前序台账已登记同一缺陷：问题扫描/账本/FIX_LEDGER.csv:232 M5a-C-001（P1/OPEN）。
- 影响：DESIGN §6.2「benchmark 生成 profile → 后续运行时自动读取」链路断裂：ISA/provider 与 workers 由硬件即时决定，benchmark 结果对生产 run 零影响；用户按帮助传 --cpu-profile 得到无效果的命令行；manifest 无法追溯本次运行依据的 profile。
- 修复面：lib/infrastructure/cli/commands.cpp:1012/1260/1656 —— 在 run 前消费 p.values["--cpu-profile"]（validate_cpu_profile + route_kernel_from_profile）并以其 workers/ISA 作为 budget/provider 来源，落 run manifest cpu_profile_path/sha256；未接入前应从命令树与 usage 移除该旗标并登记 NOT_WIRED。（只登记，不改）

### partial-cli-16 [C3] v6 模式门读 --config（命令树不存在的旗标）→ legacy weight_mode=0 的 CLI 拒绝面不可达
- 规范：`lib/infrastructure/cli/v6_runtime_contract.h:10`「//     FZ-FIELD-WEIGHTMODE legacy 0 / auto / support_x_snr2 拒绝；1|2 → baseline 非生产。」
- 实现：`lib/infrastructure/cli/v6_mode_gate.h:34`
- 判定：未实现
- 证据：v6_mode_gate.h:34 `if (p.values.count("--config")) {` 是唯一读 config 的分支；但 --config 不在 command_tree.h 的 allowed（:82-90 三命令旗标表）也不在 cmd_value_tokens()（parser.cpp:41-47），实测 ./build/astrocs mosaic --config <cfg> → rc=2 "astrocs: unknown flag '--config'"（tests/cli/test_cli001_vpi.py:102-103 同样断言 rc=2）。因此 have_doc 恒 false，:72-77 的 `weight_mode` 整数路由在生产 CLI 永不执行（emit_route("config.weight_mode") 永不发出）。
- 影响：CLI 面对 weight_mode=0（FZ-FIELD-WEIGHTMODE 禁止项）不再 fail-closed 于参数层；只剩算法层兜底（lib/algorithms/coverage/src/coverage.cpp:443、integration/v6/src/phase2_integrate.cpp:229），CLI 事件面 v6_mode_route 的 config 来源恒缺失，审计无法从 CLI 证明该门生效。
- 修复面：lib/infrastructure/cli/v6_mode_gate.h:34 —— 改为读 p.values.at("--json")（新树唯一配置旗标）解析文档；或让会话层把已解析 config 传入 mode_gate。（只登记，不改）

### partial-cli-17 [C3] 配置字段 workers/block/memory_limit/cache_budget_mb/schedule_policy 在 CLI 白名单缺失 → 一律 exit 3，无法设定线程预算
- 规范：`docs/plugins/infrastructure/19_runtime.md:63-70`「| `workers` | 由 profile | —— | 线程预算（来自 cpu_profile，不得硬编码） |」
- 实现：`lib/infrastructure/cli/parser.cpp:275-329`
- 判定：未实现
- 证据：kAllowedKeys（:275-276）={schema_version,inputs,output_dir,phase3}，kSessionKeys（:277-329）无 workers/block/memory_limit/cache_budget_mb/schedule_policy；未知键 → exit 3（:330-336）。实测 {"source":{...},"output_dir":...,"workers":4} → rc=3 "astrocs: config has unknown key 'workers'"。CLI 亦无 --workers 旗标（实测 rc=2 "unknown flag '--workers'"；command_tree.h:82-90 未登记）。
- 影响：19_runtime §5 表列出的 5 个调度/资源配置字段在生产入口不可达：用户既不能按 profile 也不能按配置设定 workers/block/内存预算，全部由 CLI 的 affinity 硬决定（见 partial-cli-15）。
- 修复面：lib/infrastructure/cli/parser.cpp kSessionKeys 增补 19_runtime §5 字段并透传到 pdoc；或在 19_runtime.md §5 标注「当前仅 profile 内部消费、非 CLI 面」并同步 schema。（只登记，不改）

### partial-cli-18 [C3] hips_browser 的 color_map 与 lod_max_order 未实现（无任何调色板/LOD order 配置）
- 规范：`docs/plugins/infrastructure/23_hips_browser.md:31`「| `color_map` | `inferno` | —— | 调色板 |」
- 实现：`lib/infrastructure/hips_browser/healpix_browser_qt/core/stf_engine.h:40`
- 判定：未实现
- 证据：全仓（lib/infrastructure/hips_browser/**）grep palette|LUT|lut_|colorMap|color_ramp 零命中；gl_renderer.cpp 只有网格线颜色（:2045-2046 uGridColor 半透明绿）。lod_max_order（23_hips_browser.md:30）亦无实现：core/ 下 grep lod 仅 hips_sky_view.h:75 `void set_lod_mode(bool strict_leaf)`，无 order 上限配置。
- 影响：显示型参数与文档约定不一致：浏览器无法按规范指定调色板/最大渲染 order，LOD 行为不可配置；文档的配置表对实现无约束力。
- 修复面：实现 color_map（默认 inferno 的 LUT）与 lod_max_order 配置项，或在 23_hips_browser.md §5 按实况订正（登记未实现项）。（只登记，不改）

### partial-cli-19 [C4] doctor 的 --json 合同为可选，实现强制要求（无 --json → rc=2）
- 规范：`docs/api/CLI_PROTOCOL_V1.md:18`「astrocs doctor [--json]」
- 实现：`lib/infrastructure/cli/commands.cpp:1871`
- 判定：不符
- 证据：commands.cpp:1871 `if (!p.flags.count("--json")) parse_fail("doctor requires --json");`；command_tree.h:96 的 allowed={"--json"} 且 help_usage（:130-133）打印 "astrocs doctor [--json]"（方括号=可选）。实测 ./build/astrocs doctor → rc=2，stderr "astrocs: doctor requires --json" + help。tests/cli/test_cli_protocol.py:115 把 ("doctor",) 列为应 rc=2 的负例，即测试把违规固化为期望。
- 影响：合同/帮助宣称的可选形态实际不可用：用户按 help 直接跑 doctor 得参数错，环境自检（docs/ci/04_ARTIFACTS.md:30「安装目录可自检：astrocs doctor rc=0」）在无 --json 调用下必然失败。
- 修复面：lib/infrastructure/cli/commands.cpp:1871 —— 允许无 --json 的人类可读输出（或在 command_tree.h/help_usage 去掉方括号并把 CLI_PROTOCOL_V1.md:18 改为必填）。（只登记，不改）

### partial-cli-20 [C4] 三个 --template 的 output_dir 默认值为 "."（进程 CWD）
- 规范：`docs/api/CLI_PROTOCOL_V1.md:70`「CLI 不得以进程 CWD(`"."`)作为隐式缺省写出,否则在工作区根散落产物并触发」
- 实现：`lib/infrastructure/cli/session_commands.h:115`
- 判定：不符
- 证据：session_commands.h:115/132/150 三处 {"output_dir", "\".\"", "运行产物唯一落点（必填非空字符串）"}；实测 ./build/astrocs {normalize,mosaic,export} --template 输出均含 "output_dir": "."。ASTROCS_DESIGN.md:429 同禁（「运行产物只落配置 output_dir，不得以进程 CWD 作隐式缺省写出」）。
- 影响：用户按模板填路径即可跑，未改 output_dir 时全部产物（run manifest、资源三件套、节点产物）落进程 CWD（仓库根），正是 CLI_PROTOCOL §7 与 UT-CLI mutates_workspace=false 要防的散落场景。
- 修复面：lib/infrastructure/cli/session_commands.h:115/132/150 把模板值改为显式占位（如 "path/to/out/<phase>"）或空串并让预检强制填写。（只登记，不改）

### partial-cli-21 [C4] 生产路径 15 处 value("output_dir", std::string(".")) CWD 兜底
- 规范：`ASTROCS_DESIGN.md:429`「- 运行产物只落配置 `output_dir`，不得以进程 CWD 作隐式缺省写出。」
- 实现：`lib/infrastructure/cli/commands.cpp:702`
- 判定：不符
- 证据：grep CWD 兜底字面量在 lib/infrastructure/cli 命中 18 处，其中 17 处是 output_dir 兜底：commands.cpp:702-703、956-957、986、1095-1096、1168、1219、1233、1331-1332、1509、1628、1690-1691 与 runtime_client.cpp:111（build_pipeline_ir 的 out_dir）；余 1 处 commands.cpp:1944 是 benchmark 的 cli_bin 兜底。全部以点号目录作缺省值。
- 影响：一旦上层校验被绕过（如 -force 路径，见 partial-cli-12）或新增调用点忘记校验，manifest/资源/产物会静默写进程 CWD；同时这些兜底掩盖「output_dir 缺失」这一配置错。
- 修复面：统一改为「缺失即失败」的取值帮助器（返回 Result/抛 ParseError → exit 2），删除所有 "." 缺省；保留 partial-cli-20 的模板占位。（只登记，不改）

### partial-cli-22 [C4] mosaic 模板默认 weight_mode=2（legacy 整数 = baseline 非生产），合同模板默认 algorithm_weight_mode=point_information（生产）
- 规范：`contracts/schemas/phase_config_mosaic.schema.json:128`「"description": "集成权重模式；默认 point_information（docs/plugins/algorithms_phase2/13_integration.md:67）；点源默认权重语义见 docs/science/PSF_SIGNAL_WEIGHT.md:28"」
- 实现：`lib/infrastructure/cli/session_commands.h:133`
- 判定：不符
- 证据：session_commands.h:133 {"weight_mode", "2", "1=等权 / 2=ivar（科学方差面）；2 要求输入含 variance/ivar 子产品，缺失即 fail-closed"}；实测 mosaic --template 输出 "weight_mode": 2。config/templates/mosaic.phase_config.json:6 为 "algorithm_weight_mode": "point_information"。而 v6_runtime_contract.h:159-165 把 legacy 整数 2 路由为 kBaseline（reason "legacy weight_mode=2 -> baseline pixel_ivar (non-production)"）。
- 影响：模板把用户默认引到非生产（baseline）权重面，且该面在缺 ivar 时 fail-closed（tests/cli/test_phase123_pipeline.py:324-332 记录 rc=2 负例）；与合同声明的生产默认（point_information）相反。
- 修复面：lib/infrastructure/cli/session_commands.h:129-145 改为默认 algorithm_weight_mode=point_information（与合同模板一致），legacy weight_mode 仅作兼容入口。（只登记，不改）

### partial-cli-24 [C4] hips_browser 显示拉伸默认 asinh，规范默认 sqrt
- 规范：`docs/plugins/infrastructure/23_hips_browser.md:32`「| `stretch` | `sqrt` | —— | 显示拉伸 |」
- 实现：`lib/infrastructure/hips_browser/healpix_browser_qt/core/stf_engine.h:40`
- 判定：不符
- 证据：stf_engine.h:40 `std::string curve = "asinh"; // 曲线预设（linear/sqrt/asinh/log）`；main_window.cpp:206 `stretch_combo_->setCurrentIndex(3);  // 默认 Asinh`、:630 `v->set_stretch("asinh", true);`。可选集合含 sqrt 但默认不是 sqrt。
- 影响：默认显示外观与规范约定不符（asinh 压缩更强）：诊断视图的对比度/可见暗部与文档描述不一致，视觉验收（ACCEPTANCE_SPEC §5.2 目检）参照物漂移；不改变科学数据（只读显示），但「不得冒充测量产品」的显示口径无法按文档复核。
- 修复面：lib/infrastructure/hips_browser/healpix_browser_qt/app/main_window.cpp:206/630 默认改 "sqrt"，或按 ENGINEERING_SPEC §3 变更 23_hips_browser.md:32 的默认值。（只登记，不改）

### partial-cli-25 [C5] resource_gate.h 仍散落字面量阈值，违反合同「实现侧不得再出现字面量阈值」
- 规范：`contracts/resource_gate_v1.json:6`「"authority_note": "判据语义（判定域/分母取义/记录与裁决划分）以该文档 §8 为唯一权威；本文件是该权威下的**唯一数值源**，实现侧不得再出现字面量阈值。",」
- 实现：`lib/infrastructure/cli/resource_gate.h:203`
- 判定：不符
- 证据：resource_gate.h:203 `double io_wait_high_percent = 50.0;  // 异常 IO 等待阈值(iowait 占比)`；:363-364 `if (g.cpu_percent < 20.0 && g.iowait_percent < 5.0 && (g.mem_bandwidth_percent < 0.0 || g.mem_bandwidth_percent < 15.0))`；:331/:358 `g.wall_seconds > 5.0` / `< 5.0`。这些数值在 contracts/resource_gate_v1.json 中不存在（合同只有 compute/memory/applicability/workload_floor 各项），resource_gate_thresholds_generated.h.in 也未生成它们。
- 影响：阈值双轨：合同（唯一数值源）改数不会影响这些内嵌判据；审计按合同复算门禁结论会与实现不一致（io_wait/cpu-low/mem-low/短任务豁免四处）。
- 修复面：lib/infrastructure/cli/resource_gate.h:203/331/358/363-364 —— 把 50/20/5/15/5.0 登记进 contracts/resource_gate_v1.json 并由 resource_gate_thresholds_generated.h.in 生成引用（或删除无判据来源的分支）。（只登记，不改）

### partial-cli-26 [C5] memory_report.h 含未登记字面量阈值（warn 2.0 MiB/s、2 s 窗口、4 样本、200 点）
- 规范：`contracts/resource_gate_v1.json:6`「本文件是该权威下的**唯一数值源**，实现侧不得再出现字面量阈值。」
- 实现：`lib/infrastructure/cli/memory_report.h:54`
- 判定：不符
- 证据：memory_report.h:54 `inline constexpr double kAllocGrowthWarnMbPerS = 2.0;`；:63 `inline constexpr double kAllocReclaimWindowSeconds = 2.0;`；:64 `kAllocMinCurveSamples = 4`；:66 `kAllocSlopeMaxPoints = 200`。合同 memory 段只有 growth_limit_mib_per_s=32 / allocation_reclaim_min_fraction=0.5 / allocation_reclaim_min_wall_seconds=10 / reclaim_residual_tolerance_bytes=33554432（contracts/resource_gate_v1.json:62-69）。
- 影响：Growing 预警线（2 MiB/s）与曲线窗口/样本门槛不受合同约束：调整合同不会同步这些判据，且 slope_points/降采样口径无法从合同复算。
- 修复面：lib/infrastructure/cli/memory_report.h:54/63/64/66 —— 数值迁入 contracts/resource_gate_v1.json 并生成引用，或在合同登记「实现内部非判据常量」豁免清单。（只登记，不改）

### partial-cli-27 [C5] module.yaml 硬编码产品版本字面量（单一版本源纪律）
- 规范：`docs/VERSIONING.md:13`「- `tools/gen_version.py`：读 `VERSION` + git HEAD/dirty → 输出合同对象与版本串。」
- 实现：`lib/infrastructure/cli/module.yaml:8`
- 判定：不符
- 证据：module.yaml:8 `module_version: 0.11.0-alpha.2`（根 VERSION 当前值 0.11.0-alpha.2，人工同步）；VERSIONING.md §2:11「生成接口（禁止多处手填）」、§4 的 check_version_consistency 扫描面为 docs/ schemas/ tools/ launch/ tests/ 与根级文件，不含 lib/。
- 影响：版本漂移不会被版本门禁抓到：lib/ 下手填版本可在 VERSION 提升后静默过期（与 reports/PROJECT-GOVERNANCE-01 W2-DATA-2 同族问题）。
- 修复面：lib/infrastructure/cli/module.yaml:8 改由构建期注入（configure_file/gen_version），或把 lib/**/*.yaml 纳入 check_version_consistency 扫描面。（只登记，不改）

### partial-cli-28 [C6] 已删除命令面的实现体与三个子命令适配器零调用者（死代码）
- 规范：`docs/plugins/infrastructure/18_cli.md:20`「- 命令树（唯一）见最高设计 §6.2：`help / --version / doctor / benchmark / normalize|mosaic|export --json|--template|--help`；」
- 实现：`lib/infrastructure/cli/commands.cpp:1726`
- 判定：待定
- 证据：零调用者清单（grep 全仓）：cmd_verify（commands.cpp:1726，唯一需要 --run-manifest，该旗标不在命令树）、cmd_phase_validate/plan/inspect（:1408/:1436/:1501，need_value("--config") 旗标不存在；仅 session_dispatch:1989-1991 的 Validate/Plan/Inspect 分支可达，而该分支无任何调用者——subcommand.h 只发 SessionOp::Run）、kConfigTemplate（:111-116，全仓唯一出现处）、normalize_subcommand/mosaic_subcommand/export_subcommand（三个头文件 :13，dispatch 走 session_commands() 表）、dispatch 的 `joined == "version"` 分支（:1851，实测 ./build/astrocs version → rc=2 "unknown command 'version'"）。命令面本身符合规范（旧命令 rc=2 实测），故「死代码」是否违规取决于 ENGINEERING_SPEC §8 退役口径（文件内注释称已按 §8 显式退役）。
- 影响：维护面：kConfigTemplate 等旧模板含 "output_dir": "."（CWD 默认）与旧命令语义，误接回生产即复活 CWD 散落；三份死代码路径的退出码映射（:1398 ARGS vs runtime_client:362 SCIENCE）与生产不一致，易被误当第二事实源。
- 修复面：按 ENGINEERING_SPEC §8 删除或显式标注 RETIRED：commands.cpp:111-116/1408-1608/1726-1820、lib/infrastructure/cli/{normalize,mosaic,export}/*.h、commands.cpp:1851 的 "version" 分支。（只登记，不改）

### partial-cli-29 [—] 命令树与 §6.2 逐行一致（help golden 实测）
- 规范：`ASTROCS_DESIGN.md:383-400`「normalize --json <config.json>              # 运行 Phase1（运行前预检 + yes 确认）」
- 实现：`lib/infrastructure/cli/command_tree.h:79-100`
- 判定：符合
- 证据：command_tree.h:79-100 登记 --version/normalize/mosaic/export/help/--help/-h/doctor/benchmark；help_text()（:141-151）由该表生成，实测 ./build/astrocs help 输出与 CLI_PROTOCOL_V1.md:12-20、tests/cli/test_cli_protocol.py:37 的 HELP_LINES 逐行一致；三命令 usage 行 "(--json <config.json> | --template [-o <path>] | --help)" 与 DESIGN §6.2 同形。
- 影响：无（口径单源、无手写副本）。
- 修复面：无需修复（保持 command_tree.h 为唯一事实源）。（只登记，不改）

### partial-cli-30 [—] 11 条退出码定义完整且数值与最高设计一致
- 规范：`ASTROCS_DESIGN.md:414-426`「| 0 | 成功且门禁全过 |」
- 实现：`lib/infrastructure/cli/exit_codes.h:7-19`
- 判定：符合
- 证据：exit_codes.h:8-18 定义 OK=0/ARGS=2/INPUT=3/SCIENCE=4/BACKEND=5/COMPUTE=6/IO=7/INTEGRITY=8/CANCELLED=9/RESOURCE=10/INTERNAL=70，与 DESIGN §6.3 表及 CLI_PROTOCOL_V1.md:34 逐条同值；protocol.h:32-36 is_frozen_exit_code_v1 用该枚举做 final.exit_code 域校验（非另抄数值表）。
- 影响：无（枚举单源；仅个别映射点用字面量，见 partial-cli-10/13）。
- 修复面：无需修复（补充：把 runtime_client.cpp 字面量改用本枚举）。（只登记，不改）

### partial-cli-31 [—] JSONL 10 必含字段 + sequence 单调 + final.exit_code 域自检硬闸
- 规范：`docs/api/CLI_PROTOCOL_V1.md:43`「- 每行必含: `schema_version,event_id,run_id,timestamp_utc,sequence,kind,severity,phase,stage,message`;`sequence` 从 0 单调递增。」
- 实现：`lib/infrastructure/cli/protocol.h:24-29`
- 判定：符合
- 证据：protocol.h:24-27 冻结 10 字段名表，:28-29 字段数由 sizeof 推导（不写字面量）；ValidateEventV1（:61-95）校验 10 字段 + sequence 期望值 + kind 扩展字段 + final.exit_code ∈ 冻结域；发送侧 jsonl.h:71-76 违规即拒发并保持 sequence 单调。
- 影响：无（协议面自检闭环；artifact 扩展字段差异另见 partial-cli-04）。
- 修复面：无需修复。（只登记，不改）

### partial-cli-32 [—] 三命令平级独立、无隐式串接入口
- 规范：`ASTROCS_DESIGN.md:63`「- 三个命令是**平级独立命令**，各自独立启动、独立恢复、独立验收；**禁止**把三阶段隐式串接为一次运行的入口。」
- 实现：`lib/infrastructure/cli/subcommand.h:356-370`
- 判定：符合
- 证据：dispatch 只接受 §6.2 三形态（--help/--template/--json），组合非法即 parse_fail；parser.cpp 的未知命令（含 run/graph/pipeline/phase1..3）→ exit 2（实测 tests/cli/test_cli_protocol.py:124-125 断言）；session_dispatch（commands.cpp:1982-1994）一次只跑一个 session；v6_runtime_contract.h:50-65 is_implicit_phase_chain 作为独立判定源。
- 影响：无。
- 修复面：无需修复。（只登记，不改）

### partial-cli-33 [—] -y 不能越过 error、-force 跳过预检（与 DESIGN §3.5 一致）
- 规范：`ASTROCS_DESIGN.md:211`「- **存在 error 时强制阻断**，**`-y` 也不能越过**，且必须报出原因；」
- 实现：`lib/infrastructure/cli/subcommand.h:304-322`
- 判定：符合
- 证据：subcommand.h:304-309 输入路径 error → rc=3（-y 不参与）；:310-315 has_error → rc=2；:316-322 仅 -y/--yes 跳过确认，EOF 视为未确认（confirm_run:37-45 fail-closed）。-force 早退见 :278-282（语义与 DESIGN §3.5:212-213 一致；与 18_cli.md 的表述冲突另见 partial-cli-37）。
- 影响：无（error 阻断面实测：export --json badtype → rc=2；-force 例外见 partial-cli-12）。
- 修复面：无需修复（先解决 partial-cli-37 的规范冲突与 partial-cli-12 的崩溃）。（只登记，不改）

### partial-cli-34 [—] benchmark 写安装目录 cpu_profile.json
- 规范：`ASTROCS_DESIGN.md:402`「- `benchmark` 直接输出 profile 到**安装目录**，后续运行时自动读取；」
- 实现：`lib/infrastructure/cli/commands.cpp:1964`
- 判定：符合
- 证据：commands.cpp:1938-1944 由 /proc/self/exe（或 GetModuleFileNameA）取安装目录，:1964 out_path = install_dir + "/cpu_profile.json"，:1971 落盘，:1973 打印路径与 verdict；实测 build/cpu_profile.json 存在（9月18日）。
- 影响：无（仅「后续运行时自动读取」半句未实现，见 partial-cli-15）。
- 修复面：无需修复。（只登记，不改）

### partial-cli-35 [—] 取消路径：协作取消 → incomplete manifest → exit 9
- 规范：`docs/plugins/infrastructure/18_cli.md:25`「- 取消：协作取消 → 关 writer → incomplete manifest → 隔离临时产物 → exit 9；」
- 实现：`lib/infrastructure/cli/commands.cpp:1694-1701`
- 判定：符合
- 证据：cancel_token.h:15-38 SIGINT/SIGTERM/控制台处理置位；runtime_client.cpp:368-383 cancel_watch 转发 Runtime::cancel()；三个会话 run 在 :1694/:1099/:1335 与 sleep 钩子内 :1638/:994/:1241 均写 status="incomplete" manifest（write_run_manifest:222-300，:253 追加 error.message）后 return astrocs::CANCELLED(9)。
- 影响：无（「隔离临时产物」由节点层承担，本层未复核）。
- 修复面：无需修复。（只登记，不改）

### partial-cli-36 [—] hips_browser 读侧复用 aio、不进入命令树/产品 manifest
- 规范：`docs/plugins/infrastructure/23_hips_browser.md:24`「- 读侧复用 aio，不复制 reader。」
- 实现：`lib/infrastructure/hips_browser/healpix_browser_qt/core/hips_browser_backend.cpp:15`
- 判定：符合
- 证据：hips_browser_backend.cpp:15 `#include "aio_hips_reader.h"`（另 browser_cli.cpp:37-38 用 aio_healpix_io.h/aio_hips_reader.h），无自建 FITS/HiPS reader；CMakeLists.txt:28-30 以 AIO_DIR=../../aio + AIO_ENABLE_HEALPIX 链接 astro_image_io；根 CMakeLists.txt 无 hips_browser 引用（grep 零命中）→ 独立组件、不进 astrocs 命令树；packaging/astrocs.product.json 无 hips_browser（grep 仅命中 dependency-lock.json）。
- 影响：无（配置默认值差异另见 partial-cli-18/24）。
- 修复面：无需修复。（只登记，不改）

### partial-cli-37 [C1] -force 语义两规范打架：DESIGN §3.5「跳过全部检查」vs 18_cli.md「只越可强制项，其余 error 仍阻断」
- 规范：`ASTROCS_DESIGN.md:212`「- **`-force` 跳过全部检查**，**直接进入运行过程**（不显示预检页面、不请求确认）。」
- 实现：`docs/plugins/infrastructure/18_cli.md:39`
- 判定：规范歧义
- 证据：DESIGN §3.5:212-213「即 -force 是"我知道我在干什么"的总开关：连 error 也一并跳过」；18_cli.md:22「仅 -force 可在缺少校准帧等情况下无阻塞运行」、:39「-force 强制越过缺失校准帧等可强制项（其余 error 仍阻断）」、:49「只有 -force 可越过可强制项」。实现按 DESIGN（subcommand.h:273-282 早退，跳过 config_structure_errors 与路径检查），subcommand.h:60 的注释却按 18_cli.md 写「即使 -force 也必须 rc=2」→ 同一文件内注释与代码互斥。
- 影响：用户对 -force 的风险面理解不同（是否连结构/路径 error 一起跳过）；实现侧因此出现 partial-cli-12 的 70 崩溃（结构检查被跳过而运行期未兜住）。
- 修复面：负责人裁决单一语义：建议 DESIGN §3.5 为准并把 18_cli.md:22/39/49 订正为「跳过全部检查」；同时把 subcommand.h:58-62 注释改为与代码一致，并按 partial-cli-12 加固运行期兜底。（只登记，不改）

### partial-cli-38 [C4] 资源门默认 record_only（不改退出码）与 18/19 的「资源门禁失败 → exit 10」无条件表述冲突
- 规范：`docs/plugins/infrastructure/19_runtime.md:82`「- 资源利用率门禁失败 → exit 10；」
- 实现：`lib/infrastructure/cli/commands.cpp:920-946`
- 判定：规范歧义
- 证据：实现默认 strict_gate=false（strict_resource_gate_arg:570-577 仅在 --strict-resource-gate/--on-resource-gate strict|enforce 时为真），违规只发 severity=warning 的 resource_gate 事件并 return OK（:927-946）；contracts/resource_gate_v1.json:74-77 "in_process_default": "record_only"、docs/plugins/infrastructure/21_observability.md:94「程序内默认 record_only：CLI 运行期只记录与报告，不因资源判据改变退出码」。18_cli.md:48「资源门禁 → 10」与 19_runtime.md:82 未提默认面。
- 影响：按 18/19 读规范会期望资源失败即 rc=10，实际默认恒 rc=0；L2 性能验收/CI 若不加 --strict-resource-gate 会把资源违约当通过（记录项与裁决项的边界只在 21/合同里）。
- 修复面：在 18_cli.md §7 与 19_runtime.md §7 增补「默认 record_only，enforce 需 --strict-resource-gate；exit 10 的充分条件见 21_observability §8.4」，或改实现默认 enforce（须负责人裁决 SO-05）。（只登记，不改）

### partial-cli-39 [C4] cpu_profile 落点两规范打架：DESIGN「安装目录」vs profile_store「平台数据目录」
- 规范：`ASTROCS_DESIGN.md:402`「- `benchmark` 直接输出 profile 到**安装目录**，后续运行时自动读取；」
- 实现：`lib/infrastructure/benchmark/backend_host/profile_store.h:51`
- 判定：规范歧义
- 证据：DESIGN §6.2 与 CLI 实现（commands.cpp:1964 install_dir + "/cpu_profile.json"）一致；但 profile_store.h:50-53 声明默认存储路径为「Windows: %LOCALAPPDATA%/AstroCS/cpu_profile.json … Linux/macOS: XDG_DATA_HOME(缺省 ~/.local/share)/AstroCS/cpu_profile.json」，profile_store.cpp:181-206 实现之。两处默认落点互斥，且二者都无生产读取者（partial-cli-15）。
- 影响：「后续运行时自动读取」无论按哪份规范都无落点：写方（CLI）与读方（profile_store 的预期消费者）指向不同文件，profile 生效链无法闭合。
- 修复面：裁决单一落点（建议保留 DESIGN 的安装目录，或让 CLI 调 default_profile_path_v1 并同步 DESIGN/19_runtime），随后按 partial-cli-15 接通读取。（只登记，不改）

### partial-cli-40 [C1] --on-resource-gate 取值域文档与诊断不一致（注释 5 值 vs 错误消息 2 值）
- 规范：`lib/infrastructure/cli/command_tree.h:62-64`「// §8/21_observability §8.4: 资源门 enforce 语义显式写法」
- 实现：`lib/infrastructure/cli/commands.cpp:576`
- 判定：待定
- 证据：command_tree.h:63 注释「（accept|record|record-only|strict|enforce）」；实现接受 accept/record/record-only（record_only）与 strict/enforce（commands.cpp:574-576），但非法值诊断只写 "invalid --on-resource-gate 'v' (accept|strict)"（:576），未列 record/record-only/enforce。21_observability.md:94 只写「`--strict-resource-gate` / `--on-resource-gate strict`」。
- 影响：诊断信息不完整：用户按文档传 record-only 合法但错误提示会让人以为只有两值；机器（GUI）无法从诊断串推导完整值域。
- 修复面：lib/infrastructure/cli/commands.cpp:576 诊断串列全 5 值；或在 command_tree.h:62-64 与 21_observability §8.4 收敛为同一值域表并生成诊断。（只登记，不改）

### partial-cli-41 [C1] 模板输出路径旗标命名：18_cli.md §5 的 `output` 字段 vs DESIGN §6.2 的 -o
- 规范：`docs/plugins/infrastructure/18_cli.md:36`「| `output` | stdout | 模板输出路径 |」
- 实现：`lib/infrastructure/cli/command_tree.h:56-58`
- 判定：规范歧义
- 证据：18_cli.md §5 表列字段名 `output`（无短名）；DESIGN §6.2:385 写「normalize --template [-o <path>]」；实现同时接受 -o / --output / --template <path>（command_tree.h:55-58，subcommand.h:329-330 只消费 -o/--output），--template 取值形态见 parser.cpp:52-54 的 bare/取值双用。三处命名面（output / -o / --template <path>）无单一权威。
- 影响：用户/文档消费者对「模板输出到文件」的旗标名不确定；--template <path> 与 --template（开关）同名两义（parser.cpp:52-54 显式处理），语义歧义面靠实现兜住。
- 修复面：在 18_cli.md §5 表把字段名改为 `-o/--output`（并在 DESIGN §6.2 补 --output/--template <path> 别名），实现侧保持不变。（只登记，不改）

### partial-cli-42 [C1] output_dir 类型错被报成「missing」（诊断口径与 §3.5「必须报原因」不符）
- 规范：`ASTROCS_DESIGN.md:204`「- **🔴 error**：**文件找不到、路径问题**、未知滤镜等。**阻塞运行**，且**强制报告原因**——」
- 实现：`lib/infrastructure/cli/parser.cpp:387-389`
- 判定：不符
- 证据：parser.cpp:384-390 对 flat_session 的 output_dir 用一条消息覆盖三种情形："config missing 'output_dir' (required for phase run; run products are written only under output_dir)"，即使键存在但类型错也报 missing。实测 ./build/astrocs normalize --json {"output_dir":123,"input_lights":[...]} -force → stderr "astrocs: config missing 'output_dir'"（实际存在且为 number）。对照 subcommand.h:67-71 的预检文案是准确的（"必须是非空字符串（收到 number）"）。
- 影响：-force 路径（预检被跳过）用户拿到误导性诊断，排查方向错误（去补键而非改类型）；与 §3.5「不得只报失败而不给理由」的原因准确性要求不符。
- 修复面：lib/infrastructure/cli/parser.cpp:384-390 —— 拆分为「缺失 / 非字符串 / 空串」三条消息（复用 subcommand.h json_type_of 口径）。（只登记，不改）

### partial-cli-43 [C3] hips_browser 迁移清单 PENDING.md 声称目录为空，实际已含 49 个实现文件
- 规范：`lib/infrastructure/hips_browser/PENDING.md:3`「- 状态：**PENDING**（本目录为空，未含任何实现；不代表模块已实现或可用）」
- 实现：`lib/infrastructure/hips_browser/healpix_browser_qt/core/hips_browser_backend.cpp`
- 判定：规范歧义
- 证据：find lib/infrastructure/hips_browser -type f | wc -l → 50（含 PENDING.md），实现位于 healpix_browser_qt/{app,core,widgets,tests,tools}（如 core/hips_browser_backend.cpp、app/main_window.cpp）；packaging/dependency-lock.json:178-180 亦把这些文件登记为 astropy-healpix 消费者（即已视为存在）。PENDING.md:7 的「旧路径仍在原处；迁移清单登记为 PENDING」与「本目录为空」自相矛盾。
- 影响：迁移/审计清单与实况不符：按 PENDING 会认为浏览器未实现而跳过其审计（本轮实际存在默认值/配置项不符，见 partial-cli-18/24）；反之 cmake/ARCH-001-migration-manifest.md 的状态也不可信。
- 修复面：更新 lib/infrastructure/hips_browser/PENDING.md:3（改为 MIGRATED/已含实现并登记实现面），同步 cmake/ARCH-001-migration-manifest.md 状态。（只登记，不改）
