# DATA-003 生产 ArtifactStore 接线（设计权威）

> owner: SA-DATA-06 · 权威文档形态（本文）+ 执行形态
> (`runtime/artifact_store/production_store.py`) + 验收测试
> (`tests/artifact/test_production_store.py`)。三形态必须同步修改。
> 前序合同: DATA-001（typed manifest schema + 唯一 producer）、DATA-002（三阶段
> 产品交换、跨 Phase 仅磁盘交换）；运行时隔离: RT-002（phase-isolated Runtime）。

## 1. 目标（tasks/03_RUNTIME_DATA_IO_TASKS.md DATA-003）

> Runtime 启动真实 Store；模块 `execute` 只能拿已校验 handle/reader/writer；
> 写临时对象 → 完整校验 → hash → 原子 publish；cancel/fail 无成功对象。
> 验收：spy Store 证明每读写经过 Store；绕过路径/producer 重复/错误 schema/
> 磁盘满/进程中断/取消均失败且可恢复；manifest hash 可重算。

约束来源: `AstroCS_ENGINEERING_CONSTRAINTS.md` A.3/A.4/A.6（三 Phase 隔离产品命令；
阶段间只通过原子发布、哈希和 provenance 完整的磁盘产品/manifest 交换）；
`13_DATA_PIPELINE_AND_ARTIFACT_STANDARD` §1（Pipeline edge 传递 ArtifactHandle，
不是路径字符串）；DATA-001 manifest 合同（storage_uri 解析只发生在 Store 内部）。

## 2. 接线结构

```text
Runtime 启动（每次 phase run）
  └─ ArtifactStore(root, run_id).start()     真实 Store（run 私有目录）
       ├─ writer 授予:   store.new_writer(id)             → Writer（临时对象）
       ├─ 暂存 manifest: store.stage_manifest(id, doc)     → ManifestRead
       ├─ 原子发布:      store.publish(id)                 → Digest（内容摘要）
       ├─ 绑定读:        store.bind_as_input(id, type)     → ManifestRead（校验后）
       └─ 校验读:        store.read_verified(id, type)     → bytes（hash 复核）
```

模块 `execute` 只接触上述 handle/reader/writer；任何真实文件系统路径解析只发生在
`ArtifactStore`/`StoreIO` 内部（DATA-001 冻结语义）。跨 Phase 消费 = 进程外读取
已发布 COMPLETE manifest + 内容（DATA-002 交换对象），不共享进程内对象。

磁盘布局（每 run 私有）:

```text
{root}/runs/{run_id}/stage/                               临时对象（未发布）
{root}/runs/{run_id}/objects/{artifact_id}                已发布内容（原子 rename）
{root}/runs/{run_id}/manifests/{artifact_id}.manifest.json    COMPLETE manifest
{root}/runs/{run_id}/manifests/{artifact_id}.manifest.sha256  manifest hash sidecar
```

## 3. 写路径（临时对象 → 完整校验 → hash → 原子 publish）

1. `new_writer(id)`：同 id 已发布 → 硬失败（唯一 producer，DATA-001）。
2. `Writer.stage_bytes(data)`：内容写入 Store 私有 `stage/` 临时文件并 fsync
   （发布前落盘；进程中断/磁盘满时不产生成功对象）。
3. `stage_manifest(id, doc)`：DATA-001 manifest 完整校验（缺字段/NaN/重复
   producer/未知 type/非法 digest/status≠COMPLETE 全拒）。
4. `publish(id)`：
   - 重读暂存内容 → sha256；
   - 与 manifest `content_digest`/`size` 核对（错误 schema/篡改 → 拒绝）；
   - manifest 规范 JSON → sha256（hash sidecar，先写 tmp 再原子 rename）；
   - 原子 rename：先内容、再 manifest（manifest rename = 完成标记）、再 sidecar；
     每步前 fsync 文件与目录。

发布物保持严格 DATA-001 manifest 形态（`additionalProperties=false`，不附加
内部字段）；manifest hash 以独立 sidecar 持久化，可重算核对。

## 4. 读路径（消费前必须经 Store 校验）

- `bind_as_input(id, expected_type_id)`：仅索引内 COMPLETE manifest + type_id 匹配
  才允许绑定（跨 Phase 资格 = DATA-002 交换资格；不要求 run ID 匹配）。
- `read_verified(id, type)`：绑定通过后经 Store 读字节并重算 sha256 与 manifest
  声明一致；不匹配 → 硬失败。
- 绕过 Store 直读 `objects/` 目录的文件不在索引内 → bind/consume 一律失败
  （无成功对象）；删除绕过文件即可恢复。

## 5. 失败语义（全部失败且可恢复）

| 场景 | 注入 | 结果 | 恢复 |
|---|---|---|---|
| 绕过路径 | 直写 objects/ 不经 Store | 不在索引；bind/consume 拒绝 | 删除绕过文件 |
| producer 重复 | 同 id 二次 publish | 硬失败（唯一 producer） | 无需（首次即成功对象） |
| 错误 schema | content_digest/size 不符 / manifest 缺字段 | publish/stage 拒绝 | 修正后重发 |
| 磁盘满 | `FailingIO`（stage_open/atomic_publish 抛 ENOSPC） | 无 COMPLETE manifest | 空间恢复后重发 |
| 进程中断 | `InterruptIO`（publish 中途抛 KeyboardInterrupt） | 无完成标记；新 Store 不索引 | 新 Store start() 后重发 |
| 取消 | writer.release() + cleanup() 不 publish | 无成功对象 | 下一 run 重发 |

无 COMPLETE manifest = 无成功对象：`start()` 恢复索引只接受
内容 + COMPLETE manifest + hash sidecar 三者齐全的对象。

## 6. spy 证明每读写经 Store

`StoreIO` 为真实 I/O 后端；`SpyStoreIO` 记录每次 `stage_open`（写）/`read_bytes`
（读）/`atomic_publish`（发布）事件。验收: 一次完整 publish + read_verified 后，
spy.writes/reads/publishes 非空，且内容字节只经 Store 事件读取 —— 模块代码路径
不含任何直接 open/read（负测对照: 绕过 Store 直读不产生成功对象）。

## 7. manifest hash 可重算

发布时对 manifest 规范 JSON（`canonical_manifest_json`，键序 = DATA-001 冻结
字段序、`ensure_ascii=False`、紧凑分隔）计算 sha256 并原子写入
`{aid}.manifest.sha256`。验收: `manifest_digest_hex(id)`（sidecar）==
`ArtifactStore.manifest_hash_recompute(doc)` == 磁盘 sidecar 内容。

## 8. 边界（非目标）

- 本任务不改科学公式/常数；不改 DATA-001/002 已冻结 schema/registry/validator/
  C ABI；不做 RT-007 checkpoint 表、不做 LOG/RT 溯源字段（DATA-004 范围）。
- 本文件为 Python 执行语义（Linux 控制/轻合成验证）；Windows 正式 DLL 交付按
  同语义 C 接线复刻，属于 DATA-004/WIN 后续范围。
