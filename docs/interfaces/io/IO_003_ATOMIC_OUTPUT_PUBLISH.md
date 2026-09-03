# IO-003 原子 HiPS/manifest 输出发布冻结合同

> doc_id: DOC-IO-INTERFACE-003
> doc_status: ACTIVE_NORMATIVE
> task_id: IO-003 · wave: W2 · owner: SA-IO-07
> commit: `feat(io): IO-003 实现原子产物发布`（前台集成）
> source: `tasks/03_RUNTIME_DATA_IO_TASKS.md` IO-003 / `05_FIXED_SUBAGENT_BINDINGS.yaml`
> SA-IO-07 / 冻结约束 `AstroCS_ENGINEERING_CONSTRAINTS.md` F.3（DLL C ABI 边界）、
> A.3/A.4（阶段隔离与原子性）、E（科学公式不变）
> 上游: DOC-IO-INTERFACE-001（IO-001 FITS 流式接口，fitsverify 算法族）、
> DOC-IO-INTERFACE-002（IO-002 HiPS 输入合同，读端接收本任务产出）、
> DATA-003（生产 ArtifactStore 原子 publish 语义）、DATA-004（provenance sidecar）

## 1. 目标与范围

IO-003 在 IO-001（FITS 原子写）+ IO-002（HiPS 读端）之上建立 **原子 HiPS/manifest
输出发布** 合同（W2 宿主基础设施，非 W3 科学迁移）：把"磁盘上产出的 HiPS 子产品目录
（properties + NorderK/DirD/NpixN.fits tiles + 可选 Moc.fits）"以 **原子、可恢复、
唯一目标** 的方式发布，并在发布完成后落 **完成 manifest**（唯一完成标记）。IO-002
读端/跨 Phase 消费只接受本任务发布的完整产物（DATA-002 R-DISK-ONLY）。

发布流水线（每个产物文件）：
`临时写（run 私有 stage）→ 关闭/fsync → fitsverify（结构 + DATASUM）→ sha256 →
原子 rename → 最后原子落 manifest.json(COMPLETE) = 完成标记`。

本任务**不改科学公式**（`scientific_change=false`），不做 tile 生成/投影（科学层属
P1/P2/P3）；`lib/astro_image_io` 与 `lib/io` 保持原样不修改。本任务冻结**输出端**语义；
读端（IO-002）与产物交换资格（DATA-002）是独立冻结面，不在此重复。

## 2. 模块归属与目录

| 内容 | 路径 |
| --- | --- |
| 本冻结合同 | `docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md` |
| 原子输出发布器（Python 执行形态） | `runtime/io/hips_output_store.py` |
| FITS 独立校验器（fitsverify，与 fits_core 同算法） | `runtime/io/fits_verify.py` |
| 契约/负测（Python） | `tests/io/test_hips_output_contract.py` |
| 测试 tile fixture（复用 IO-001 fits_core） | `tests/io/hips_output_fixture.py` |

允许写路径：`runtime/io/** modules/services/io/** tests/io/** docs/interfaces/io/**`。

> 执行形态说明：`runtime/io/hips_output_store.py` + `fits_verify.py` 为纯 Python
> 语义层（Linux 控制/轻合成节点可完整验证；与 DATA-003 production_store 同模式）。
> Windows 正式 DLL 交付（astrocs_io.dll）由 IO-003 同语义 C 接线复刻同一发布状态机；
> manifest/tree hash/错误码公式不变，跨 DLL 边界不暴露路径字符串句柄。

## 3. 唯一目标与 run 隔离

### 3.1 目标布局

```
{output_root}/runs/{run_id}/stage/                    # run 私有临时写区（未发布）
{output_root}/runs/{run_id}/products/{user_path}/     # 已发布目标（唯一用户路径）
{output_root}/runs/{run_id}/products/{user_path}/manifest.json   # 完成 manifest
```

- `run_id` 词法：`^[A-Za-z0-9._-]+$`（唯一；两个不同 run 的目录树完全分离，
  **并发不同 run 绝不共享目标路径** → 不互相覆盖）。
- `user_path` = 用户路径（相对；可含子目录 `signal`、`signal/Norder0/…` 等
  目录型产物），段词法 `^[A-Za-z0-9._-]+$`。

### 3.2 词法拒绝（路径穿越）

`user_path` / 发布文件名 / run_id 含以下任一 → `PathTraversalError`（发布前拒绝）：

| 违例 | 例子 |
| --- | --- |
| 绝对路径 | `/abs/path` |
| 父目录穿越 | `../evil`、`a/../../b` |
| 空段 / 当前段 | `a//b`、`a/./b`、尾 `/` |
| 反斜杠（Windows 分隔符） | `a\b` |
| 非法字符段 | `a b`、`a*b`（段外 ASCII 可见字符一律拒） |

文件名（products 内相对名）额外约束为 HiPS 目录产物形态：
`properties` | `Moc.fits` | `NorderK/DirD/NpixN.fits`（K/D/N 为十进制数字）——
其它文件名（如 `notes.txt`、任意 `.fits` 布局）发布前拒绝（`PublishError`）。

### 3.3 文件系统层拒绝（权限/符号链接）

发布器在每次写/rename 前对目标路径组件做符号链接检查：任何已存在组件为
符号链接 → `PermissionError_`（防符号链接逃逸 stage/目标根）。底层 I/O 层
把 EACCES/EPERM/EROFS/EISDIR 统一映射为 `PermissionError_`；发布产物目录
权限收紧 `0o750`、文件 `0o640`（阶段隔离，绝对用户路径/凭据不落盘）。

## 4. 发布流水线（原子语义）

一次 `publish_directory(user_path, files, overwrite=…)`：

1. **先检后写**：`user_path` 词法 → 目标存在性 → 文件清单词法（properties 必在；
   文件名形态）→ 符号链接检查。全部通过才进入写。
2. **覆盖清理**：默认不覆盖——目标为**成功对象**（含 COMPLETE manifest）且
   `overwrite=False` → `PublishError`。中断/cancel 残留（目录存在但无 COMPLETE
   manifest）= **非成功对象**（DATA-003 语义）→ 自动清残重发；成功对象仅在显式
   `overwrite=True` 时清除后重建。
3. **stage 临时写**：每个文件写入 `{run}/stage/`（O_EXCL；临时名带内容 sha256
   前缀防碰撞）；写后 `fsync` 关闭（关闭 = 内容完整落盘）。
4. **fitsverify**：`.fits` 科学平面 tile（`NpixN.fits`）必须通过结构 + DATASUM
   校验（`fits_verify.py`，与 IO-001 fits_core `fits_verify_file` 同一算法族，
   可由 C verifier / astropy 交叉验证）。失败 → `PublishError`，无成功对象。
   `Moc.fits` 为 BINTABLE（IO-001 支持域外，IO-002 MOC optional）→ 只做 sha256
   + 原子落盘，不阻塞发布。
5. **sha256**：每个文件内容 sha256/64hex 记录。
6. **原子 rename**：逐个 `os.replace`（同文件系统原子）从 stage → 目标；
   每步前已 fsync。
7. **完成 manifest**：最后原子写 `manifest.json`（`status=COMPLETE`；= 唯一完成
   标记）。成功对象 = 内容 + COMPLETE manifest 齐全。

任一步失败/中断（KeyboardInterrupt/SystemExit/异常）→ 无 COMPLETE manifest、
无成功对象；可恢复（`cleanup()` 清 stage / 新 Store `start()` 不索引残留 /
同 run 重发）。

## 5. 完成 manifest 形态

```json
{
  "manifest_schema": "astrocs.hips-output-manifest/v1",
  "manifest_version": 1,
  "status": "COMPLETE",
  "run_id": "run-abc123",
  "user_path": "signal",
  "product": "signal",
  "publisher": "astrocs.hips-output/v1",
  "tree": [
    {"path": "Norder0/Dir0/Npix0.fits", "size": 20160, "sha256": "<64hex>"},
    {"path": "properties", "size": 87, "sha256": "<64hex>"}
  ],
  "tree_hash": "<64hex = sha256(规范 tree)>",
  "fitsverify": {"performed": true, "checksum": "datasum", "tile_count": 1},
  "created_utc": "2026-09-02T08:15:00Z",
  "producer": {"module_id": "...", "module_build_id": "..."}
}
```

- `tree` 条目 = `{path, size, sha256}`（稳定排序）。
- `tree_hash` = sha256(规范 JSON 序列化的 tree 条目数组) → **可重算**：
  同内容重算一致；任何文件改动/增删 → hash 变化。重算 = `tree_hash(tree_entries)`
  或 `recompute_tree_hash(manifest)` 或按磁盘实际文件重算 `verify_tree_hash()`。
- `fitsverify` 记录发布时已执行校验（performed/tile_count）——机器证据。
- `producer`（可选）由调用方注入；不含绝对路径/凭据（privacy：任何绝对
  Unix/Windows 路径不进入 manifest —— 结构上字段均为词法受限标识）。

## 6. 错误语义

| 情形 | 结果 |
| --- | --- |
| 路径穿越（词法） | `PathTraversalError`（发布前；无写入） |
| 目标为成功对象且 overwrite=0 | `PublishError`（默认不覆盖） |
| 符号链接组件 / 权限拒绝 | `PermissionError_`（无成功对象） |
| tile fitsverify 失败 | `PublishError`（无成功对象） |
| 中断（进程/注入） | 无 COMPLETE manifest（可恢复） |
| 成功 | COMPLETE manifest 唯一完成标记 |

任何失败都不产生成功对象：无 COMPLETE manifest、不入索引、不可消费、可恢复。

## 7. 验收映射

| 验收（tasks IO-003） | 覆盖 |
| --- | --- |
| 每个输出以唯一用户路径或 run ID 目录 | §3.1 + `TestUniqueRunDirIsolation` |
| 临时写、关闭、fitsverify、SHA256、原子 rename、最终完成 manifest | §4 + `TestAtomicPublishPipeline`（spy 证据） |
| 默认不覆盖，显式 overwrite 才可 | §4.2 + `TestNoOverwriteDefault` |
| 并发不同 run 不互相覆盖 | §3.1 + `TestConcurrentRunsIsolated` |
| 中断后无完成标记 | §4.7 + `TestInterruptNoCompleteMark` |
| 文件权限/路径穿越拒绝 | §3.2/3.3 + `TestPathTraversalRejected`/`TestPermissionRejected` |
| tree hash 可重算 | §5 + `TestTreeHashRecomputable` |
| fitsverify 与 C verifier 同一判定 | `TestFitsVerifyCrossOracle` |

## 8. 已知限制

1. Linux 控制节点（本任务执行环境）无 MSVC/Windows DLL 构建；产出 Python 语义层 +
   全部契约/负测。Windows 正式 DLL 构建（astrocs_io.dll）在 W6 用同一状态机源码执行。
2. `Moc.fits`（BINTABLE 扩展）不做内容校验（IO-001 §14.2：表扩展 UNSUPPORTED；
   IO-002 MOC optional hint 语义：缺失/损坏不阻塞读/写）。
3. 并发安全以 run 目录隔离 + 单次发布单线程为前提；单 run 内并发发布同一
   user_path 由调用方串行化（与 DATA-003 writer 唯一 producer 同纪律）。
4. CHECKSUM 卡写路径沿用 IO-001 默认关闭；fitsverify 校验 DATASUM（写入侧恒写
   DATASUM 卡），CHECKSUM 卡存在且非占位时同样校验。
