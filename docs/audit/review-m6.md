# M6 独立复核（锚点 `53cff237f`）

> 复核者：reviewer-m0m4（未参与实现）。只做只读检查与 `/tmp` 探针，未修改仓库代码/用例，未 commit/push。
> 锚点：`53cff237fb2fdd33aa4530a214c1925572e84e3e`（M6 全链提交）。隔离方式：`git archive 53cff237f → /tmp/review-m6`，Ninja 构建 OFF（DEV 另建）；`ctest` **20/20**，runtime 套件 **16/16**，`check_design` PASS。全链在 `/tmp/m6run` 的 X01 副本上用隔离二进制重跑，输出写到 /tmp，不触碰仓库。

## 结论

**X01 的 normalize→mosaic→export 发布链：ACCEPT-WITH-CAVEATS（可复现的 PASS）。**
**M6 里程碑整体：未关闭（PENDING）。** 证据支持“Linux 上合成 X01 三命令发布真实产品”，不支持 DEVELOPMENT.md:15 定义的完整 M6（C01—X01 全路径、双平台 baseline、干净安装/恢复、性能与故障报告）；X01 case 自身的真值比较仍是 `pending_implementation`（见 P1-2）。

- **P0：无。**
- **P1：2 项**（astrometry 手性硬剔除；X01 case 真值/科学判据未跑，发布 smoke 不能替代 M6 验收）。
- **P2：若干**（证据时效/可追溯、BlockStore persistent 生命周期、provider 静默 XP、bridge fallback、e2e 判据强度、footprints 缺失回退）。

## 已复现的事实

1. **隔离 HEAD 全链重跑**：`/tmp/m6run`（干净 `products/`，mosaic/export 的 `expected_hash` 由脚本按新产物 manifest 重绑）→ normalize **8/8 published**、mosaic **1**、export **1**，三命令 exit 0，`stdout_is_pure_jsonl=true`，无 foreign 行，`status=pass`，耗时约 **41 分钟**（`e2e_x01.py` 的 3600s 超时内；Lead 报 ~50 分钟，属同量级）。
2. 8 个 normalize 产品 + mosaic + export 的 `manifest.complete=true`，均有 `signal/`（export 为 `signal.fits`）；mosaic 的 4 个 `expected_hash` 与 4 个 A-frame `manifest.json` 的 sha256 逐一相等；export 的 `expected_hash` 与 mosaic manifest 相等；`src/workflows/product.cpp:1009-1013` 在打开产品时校验 `location.manifest_hash`，链路绑定成立。
3. `e2e_events.jsonl`：normalize 一个 run（8 个 frame_finished，x01-00…07，全部 ok/published）、mosaic 一个 run（inputs_total=4）、export 一个 run（inputs_total=1）；无重复帧、无跳过命令。
4. 脚本先 `shutil.rmtree(CLI_DIR/"products")`（`e2e_x01.py:109`），确为干净输出树；输入 frames 从冻结 `input/frames` 复制（`cli_inputs.py:write_inputs`），配置由脚本确定性重写；未发现硬编码 PASS 或跳过步骤。
5. runtime 16/16、`ctest` 20/20（HEAD 比 Lead 报的 19/19 多一个 test，不是缺失）、`design_integrity` PASS。

## P1-1：astrometry 的 `affine_det > 0` 硬剔除过强，漏掉合法镜像手性

- 位置：`src/sources/astrometry.cpp:668-674`
  ```cpp
  const double affine_det = det00 * det11 - det01 * det10;
  ...
  if (affine_det > 0.0) continue;   // 仅保留 orientation-preserving
  ```
- 正本要求相反：`docs/engineering/rebuild/processes/sources.astrometry.md:46` “镜像均尝试”；同文件 `:78` “按…镜像序形成稳定 hypothesis 序”；`docs/engineering/rebuild/processes/coordinates.wcs.md:34` 把“镜像CD”列为必须通过的 WCS 反例/测试。一个带反射的仿射**仍是合法 WCS**（CD 行列式符号由像素/天球轴约定决定，并非只能是负）；该注释“reflected affine … cannot be a WCS”在科学上不成立。
- **A/B 复现**（/tmp，未改仓库）：把 `sources.astrometry` 驱动副本的 `CD1_1` 由 `-0.0005` 改为 `+0.0005`（同一冻结源场，catalogue 由同一 WCS 正向投影生成）：
  - HEAD 隔离二进制：`astrometry.solved=false`，`issue="unsolved: solve_astrometry: the candidate matched fewer sources than the required minimum"`；
  - 在 `/tmp/review-m6` 副本去掉 `if (affine_det > 0.0) continue;` 后重建 `sources`：`astrometry.solved=true`，`cd_error=1.18e-17 rad/pix`，`crpix_error=4.58e-05 FITS px`。
- 影响：X01 的 A/B 帧真值 CD1_1=-0.0002777778（det<0），所以本次 X01 PASS 不受影响；但任何真实镜像光学/翻转读出/镜像坐标系都会从“可解”退化为 unsolved，且与 process 正本冲突。
- 建议：删除该全局手性剔除，保留“镜像均尝试”，让已有的全源匹配/残差验证淘汰错误 chirality；若确实要限制，应由 `SolverRequest` 显式携带 chirality 约束（默认两者），而不是硬编码。

## P1-2：X01 的科学比较仍是 `pending_implementation`，“发布 smoke”不等于 M6 验收

- `validation/x01/comparison.json`：`status=pending_implementation`，唯一 functional `three_commands_on_disk` 的 `reason="the three command entry points are not available yet"`（已过时：CLI 已存在并跑通）；`case.json` 的 positive/negative expectations 与 tolerance 未被执行：
  - `blind_solve`：无 WCS 组必须真盲解并**报告残差**（`tolerance.astrometry_rms_pixel=0.5`）；
  - `source_recovered`：点源在注入位置附近恢复（`signal_relative=0.05`）；
  - `no_fabricated_solution`：盲解失败必须 unsolved 且不发布（负例）。
- `e2e_summary.json` 只有 published/unsolved/failed 计数与 exit code；**没有** astrometric residual、源位置/通量恢复、负例注入。`e2e_x01.py:135` 的 verdict 只要求三命令 exit 0 且 stdout 纯 JSONL；它不检查 `published==inputs_total`、`failed==0`、`unsolved==0`，也不检查上述 science expectations。
- 虽然 CLI 的 exit code 编码了发布结果（normalize：`published < inputs.size()` → `kPartial=6`，见 `apps/cli/commands.cpp:1211-1217`；mosaic/export 仅在发布后 `kSuccess`，见 `:985`、`:1339`），因此 exit 0 事实上蕴含本次“全部发布”；但**这不覆盖 case 的盲解残差/源恢复/负例**。把该 e2e 称为 “M6 全链 PASS” 会超出证据范围；应实现并运行 X01 truth 比较（`truth_x01.py` 的 comparison payload 已有框架），或明确把 M6 验收拆成“发布链 PASS”与“科学 case 待办”。

## M6 里程碑本身：未关闭

- `docs/engineering/rebuild/standards/DEVELOPMENT.md:15` 的 M6 要求：C01—X01 所需完整路径逐项实跑、**双平台** baseline、**干净安装/恢复/性能与故障报告**。本轮只有 Linux 合成 X01 发布链；无 Windows、无干净安装/恢复、无性能报告（50 分钟且明确未声称生产速度）、无故障/负例报告。
- `docs/audit/IMPLEMENTATION-STATUS.md:29` 仍记 M6 为 **PENDING**（“完整当前 Normalize→publish→new-process open→Mosaic→Export、双平台、性能与 GUI 协议验收”），与本次证据一致。
- 因此：**发布链 ACCEPT-WITH-CAVEATS；M6 整体不判 ACCEPT**。

## P2 清单（按风险排序）

1. **BlockStore persistent 生命周期：跨 command 泄漏（无 UAF 证据）。**
   - `src/runtime/internal.hpp:291-293` 的 `persistent` 语义；`src/runtime/storage.cpp:1905-1918`：persistent 只减 refs、**永不 erase**；`close_command`（`src/runtime/pool.cpp:1460`）不清 `entries_`；`register_array` 的 `entry.persistent = entry.persistent || persistent`（`storage.cpp:1897`）。
   - 与 `internal.hpp:274-278` 的注释直接矛盾：“release_ref erases … exactly when the last reference dies, so an array is never pinned by the cache after every ArrayRef is gone”——persistent 条目正是被永久 pin 住。
   - 影响：CLI/normalize 每个 command 新建 `Services`、进程结束即销毁，故本次 X01 不受影响；但同一 `Services` 上多 command（长驻 Host）会累积输入数组及其 `Lease`/claim，claim 仍挂在已 close 的 persistent arena 的 scope 上，容量不归还。
   - UAF 检查：未发现。Entry 持有 `shared_ptr<const vector<byte>>`、`Lease`（ArrayOwner）与 `ClaimImpl`（`ArenaImpl`/`ScopeRef` 均为 shared），array/arena/scope 都比 entry 活得久；`release_ref` 对 persistent 不 erase，不会 double-free；bridge fallback 的 claim 也由 ArrayOwner 持有。建议：在 command close 时清 persistent entries（或引入 command 级 BlockStore/显式 `clear_persistent`），并给 `close_command` 加腾空/容量断言。
2. **provider 缓存静默丢 XP。** `src/catalogue/provider.cpp:186-202` 的 `read` 失败返回空 vector，`load_index_arrays` 不报错；`make_star` 仅在 `!xp_coefficients_.empty()` 时构造 XP（`:307-328`），缓存空时直接返回 ok 且 `star.xp_coefficients` 为空——把“读失败/未加载”静默降级成“该星没有谱系数”，与 `04af17cb7` 的 “failed XP row read is missing_dependency, never a silent null” 相反。建议：`load_index_arrays` 传播读错误（构造失败），或 `make_star` 在 `coefficients>0 && cache.empty()` 时返回 `missing_dependency`。
3. **证据时效/可追溯性。** committed `validation/x01/observed/e2e_summary.json` mtime **02:04:22**，而 `src/sources/astrometry.cpp` mtime **02:06:08**、`/tmp/acsd-build/acsd` mtime **02:06:15**，提交时间为 **02:10:02**：committed PASS 不是由 HEAD 的最终代码生成的（astrometry 手性剔除在 run 之后才加入）。我用 HEAD 隔离二进制重跑仍 **8/8/1/1 PASS**，所以结论可复现；但 committed 证据本身已过时，且 summary 只记相对 binary 路径，未记 `git rev-parse HEAD`/binary sha256/frames 输入哈希。建议：最终代码后重跑并把这些身份写入 summary。
4. **e2e verdict 强度。** `e2e_x01.py:135` 仅 `exit_code==0 && pure`；建议显式断言 `published==inputs_total`、`failed==0`、`unsolved==0`，并把 `stages` 的 counts 作为 pass 的必要条件（当前靠 CLI exit code 间接保证）。
5. **bridge fallback 弱化 persistent 承诺。** `src/runtime/science_bridge.cpp:61-83`：persistent arena 满时回落到 segment arena 并 commit 到该段；数组靠 claim 存活，无 UAF，但该数组的容量记在可能已 close 的 segment scope 上，“persistent” 语义不再成立。建议：要么 persistent 失败即具名失败，要么明确 fallback 数组不得跨 invocation 别名。
6. **ingest 早退路径不标记 persistent。** `src/workflows/normalize_runtime.hpp:66-72`：若 id 已在 BlockStore（可能是 bridge 的非 persistent 输出），只记 `ingested` 就返回，不走 `register_array(..., true)`。若该条目随后 refs 归零被 erase，后续读会 missing_dependency（非 UAF）。建议：早退时对该 id `add_ref`/标记 persistent。
7. **normalize 中心在 footprints 缺失时的回退。** `src/workflows/normalize.cpp:570-586`：`width/height` 仅在 `observation->footprints` 存在时赋值；缺失时中心退化为 (0.5,0.5)、半径 1e-3，查询会落在角上。X01 的 regroup 总是给 footprints，故本次不触发；建议明确要求 footprints 或回退 CRVAL/探测器尺寸。
8. **`inputs.directory` 为绝对路径**（`e2e_summary.json`），换机器不可复现；建议相对仓库或记录 input model/truth hash（`case.json` 已有 `input_model_sha256`）。

## 建议的最小收尾

1. 删除 astrometry 的全局 `affine_det > 0` 剔除（P1-1），保留镜像枚举与全源匹配验证；补一个镜像 CD 的正例（`coordinates.wcs.md:34` 的 C01/G01 镜像测试）。
2. 实现/运行 X01 truth 比较（盲解残差 ≤0.5 px、源恢复 5%、negative no_fabricated_solution），并把 `comparison.json` 从 pending 更新；或把 M6 验收声明拆分为“发布链 PASS”与“科学 case 待办”。
3. 最终代码后重跑 e2e，summary 记录 `git rev-parse HEAD`、binary sha256、frame/truth hashes；显式断言 published/failed/unsolved。
4. 处理 BlockStore persistent 的 command-close 清理与 provider 的 XP 错误传播（P2）。
5. M6 里程碑仍按 DEVELOPMENT.md:15 补双平台、干净安装/恢复、性能/故障报告；在补齐前不要写“M6 完成”。

## 复现命令（/tmp）

```bash
# 隔离构建
mkdir -p /tmp/review-m6 && git archive 53cff237f | tar -x -C /tmp/review-m6
cmake -S /tmp/review-m6 -B /tmp/review-m6-build -G Ninja -DACSD_BUILD_SCIENCE=ON -DACSD_DEV_MODE=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /tmp/review-m6-build -j 8
(cd /tmp/review-m6-build && ctest)                       # 20/20
# 全链重跑（干净输出树；隔离二进制）
rm -rf /tmp/m6run && mkdir -p /tmp/m6run/validation /tmp/m6run/build/science
cp -a /tmp/review-m6/validation/x01 /tmp/m6run/validation/x01
ln -s /tmp/review-m6-build/acsd /tmp/m6run/build/science/acsd
cd /tmp/m6run && python3 validation/x01/e2e_x01.py       # 8/8 + 1 + 1, status pass
# astrometry 镜像 A/B（/tmp 副本，仅探针）
# 1) 复制 validation/process/sources.astrometry/astrometry.cpp，CD1_1 改 +0.0005
# 2) 用隔离库编译运行：HEAD 二进制 -> astrometry.solved=false
# 3) 在 /tmp/review-m6 副本删除 src/sources/astrometry.cpp:674 的 `if (affine_det > 0.0) continue;`
#    重建 sources，重跑同一驱动 -> solved=true, cd_error≈1.18e-17, crpix_error≈4.6e-5
# runtime 16/16
cd /tmp/review-m6 && ACSD_BUILD=/tmp/review-m6-build ACSD_DEV_BUILD=/tmp/review-m6-dev bash validation/runtime/build.sh
ACSD_BUILD=/tmp/review-m6-build ACSD_DEV_BUILD=/tmp/review-m6-dev python3 validation/runtime/run_cases.py --no-build
```

**复核签署**：reviewer-m0m4。仅记录证据与建议；未改仓库代码/用例，未 commit/push。

---

# 追记：整改后定点复核（2026-10-25 05:00，锚点 `2855a521b` / 代码 `cbd4bcc0c`）

> 代码提交 `cbd4bcc0c`；`2855a521b` 只新增最终 e2e 证据与配置哈希。隔离重建 `/tmp/review-m6b`（`git archive 2855a521b`），`ctest` **20/20**（隔离构建；仓库 `build/science` 为 19/19，见 §4）、runtime suite **16/16**、`design_integrity` PASS。未改仓库代码/用例。

## 1. P1-1 手性：**CLOSED**

- `src/sources/astrometry.cpp:668-674` 的 `if (affine_det > 0.0) continue;` 已删除，改为“两种手性都是合法 WCS，由全源匹配决定”；与 `processes/sources.astrometry.md:46`（镜像均尝试）、`:78`、`coordinates.wcs.md:34`（镜像 CD 测试）一致。
- **镜像 CD 复现**：把驱动副本 `CD1_1` 设为 `+0.0005`（同一冻结源场，catalogue 由同一 WCS 正向生成），用 HEAD 隔离库编译运行 → `astrometry.solved=true`，`astrometry.cd_error_rad_per_pixel=1.1822387905537114e-17`，`crpix_error_fits_pixel=4.582623304116851e-05`，19/19 checks pass（与上轮“删 guard 后”的 A/B 数值逐位一致）。
- 标准帧驱动（`CD1_1=-0.0005`）仍 `solved=true`，`cd_error=3.49e-16`、`crpix_error=2.44e-05`，19/19 pass。

## 2. P2 provider XP：**CLOSED（代码审读）**

- `src/catalogue/provider.cpp:172` 新增 `xp_cache_failed_`；`load_index_arrays`（`:193-206`）在 decoded size ≠ `rows*coefficients` 时清空缓存并置失败。
- `make_star`（`:309-323`）在 `coefficients>0 && xp_cache_failed_` 或 `columns->xp_coefficients` 存在但缓存为空时返回具名 `missing_dependency`，不再静默返回无 XP 星；正常路径仍按行 copy。
- 残余（P3）：若 `coefficients>0` 但 `columns->xp_coefficients` 为空指针，仍走“无 XP”而不是报错；从列元数据构造看这属于产品不一致的边角，建议顺手拒绝。

## 3. P2 persistent 清理：**仍未关闭，标 CAVEAT（不阻塞 X01 发布链 / M6 不因此 ACCEPT）**

- 现状：`BlockStore` 的 persistent 条目（`internal.hpp:291-293`）在 `release_ref` 只减 refs、永不 erase（`storage.cpp:1909-1912`）；`close_command`/command drain 不清 `entries_`。整改尝试在 drain 回调里清 persistent 导致 teardown 死锁，已回退（commit message 记录；代码无残留）。
- **是否阻塞 M6**：不阻塞“Linux 合成 X01 三命令发布链”这一已证事实——CLI 每个命令一个进程/一个 `Services`，normalize/mosaic/export 各自结束时 `Services` 销毁，persistent 条目不跨命令累积；mosaic/export 另起进程。它是**长驻 Host 在同一 `Services` 上多次 `open_command`** 的资源泄漏（输入数组 + Lease/claim，claim 还挂在已 close 的 persistent arena scope 上），属于运行时 API 的 P2 鲁棒性/资源项，不是 X01/M6 发布链的 P0/P1。
- 必须同步的文档：`internal.hpp:274-278` 仍写“release_ref 在最后引用死亡时 erase、不会被 cache 永久 pin”，对 persistent 已不成立；请改为明确 persistent 例外，并补一个多 command 生命周期测试。若后续 M6“干净安装/恢复”会用同一个 Host 连续跑多 command，则此 P2 需升级为该项的前置条件；在当前证据范围内标 CAVEAT。

## 4. P1-2 科学比较 / 发布链表述：**同意“发布链 PASS / M6 PENDING”**

- `validation/x01/comparison.json` 现在：整体 `status=pending_implementation`，`three_commands_on_disk=pass`（引用 e2e summary），`blind_solve`/`source_recovered`/`no_fabricated_solution` 三项 `pending_implementation` 并写明原因（残差未持久化、无 truth-to-product oracle、负例未注入）；`notes` 明确“publication smoke test 不是 science pass”。这解决了上轮“用发布 smoke 冒充 M6 全链”的 P1-2，**不再算过度声明**。
- `e2e_x01.py:134-140` 的 verdict 已加强为 `exit_code==0 && published>0 && unsolved==0 && failed==0 && pure`；normalize 的 exit 0 仍经 `kPartial=6` 蕴含 8/8（`apps/cli/commands.cpp:1211-1217`），mosaic/export 的 `kSuccess` 也只在发布后（`:985`、`:1339`）。
- 不建议把 comparison 整体写成 pass：当前 pending 是诚实的；科学三项（盲解残差 ≤0.5 px、源恢复 5%、无伪造解负例）仍是 M6 的**验收缺口**，不是“发布链 PASS”能替代的。M6 里程碑仍 PENDING（另含双平台、干净安装/恢复、性能/故障报告；`DEVELOPMENT.md:15`、`IMPLEMENTATION-STATUS.md:29`）。

## 5. X01 8/8 与证据溯源

- committed `validation/x01/observed/e2e_summary.json`（在 `2855a521b`）：`status=pass`、`git_commit=cbd4bcc0c…`、`binary_sha256=07fdb04c…`、`frame_hashes` 8 个；stages 为 normalize 8 / mosaic 1 / export 1，exit 0，unsolved/failed 0。
- 我核对了溯源：`07fdb04c…` 等于实际 `/tmp/acsd-build/acsd`（mtime 03:43:40，晚于 fix commit 03:42:21）；8 个 `frame_hashes` 与 `validation/x01/input/cli/frames/*.fits` 逐一相等；`e2e_events.jsonl` 为 28 事件（8 frame_finished + 2 product_published + 3 run_finished），run_id 三个、帧 x01-00…07 + mosaic/export 完整。**发布链 8/8 的结论与证据自洽**。
- 本轮未重复跑完整 8 帧（约 40 分钟；我在 `/tmp/m6run2` 启动后于 x01-00 处停止以控制时间），但我独立验证了受本次改动影响的 astrometry 镜像/标准路径（上节），且 committed 证据的 binary/git/frame 哈希与 HEAD 代码一致。若需要“独立完整重跑”作为最终发布证据，建议在安静环境重跑并保留 `git_commit`/binary sha（脚本已支持）。

## 6. 测试计数与库存

- 隔离 HEAD `ctest` 为 **20/20**；仓库 `build/science` 的 `ctest -N` 为 **19/19**，差一个是 `process_photometry.fit_x01_minimal_repro`（已存在的 driver，仓库 build 目录未重新 configure/构建该 target）。两边都全过；Lead 的“19/19”不算错，但 HEAD 的完整测试清单是 20 个，建议重新 configure 后按 20/20 记录。
- runtime suite 16/16（含 d01_on DEV、multi01、storage01、ASan 的 life01/io01/b01）在本轮隔离构建复现；`design_integrity` PASS。

## 7. 其他 P2/P3（不影响本轮结论）

- `docs/audit/m0-implementation-matrix.md:213` 的 L04 仍写 “`acsd normalize` 端到端 published=0”；发布链现在 8/8，该行应改为“发布链 8/8，科学比较 pending”，否则与 e2e 证据矛盾。
- `comparison.json` 的 `status=pending_implementation` 语义是“public entry point 未实现”，而这里 pending 的是科学比较器/负例；notes 已解释，属 P3 措辞。
- e2e verdict 用 `published>0` 而非 `published==inputs_total`；normalize 的 exit code 已隐含全发布，属 P3 防御性建议。

## 8. 结论

| 项 | 状态 |
|---|---|
| P1-1 astrometry 手性 | **CLOSED**（镜像 CD solved=true，cd 1.18e-17、crpix 4.58e-5；标准帧仍 solved） |
| P2 provider XP | **CLOSED**（代码审读；残余 null-array 边角 P3） |
| P2 persistent 清理 | **OPEN → CAVEAT**（不阻塞 X01 发布链；长驻多 command Host 泄漏；需修注释+生命周期测试） |
| P1-2 科学比较/表述 | **CLOSED（作为过度声明）**；科学三项仍 pending，是 M6 验收缺口 |
| 证据溯源 | **CLOSED**（git_commit/binary sha/frame hashes 与 HEAD 代码一致） |
| X01 8/8 | **committed e2e 在 cbd4bcc0c 自洽**；本轮未重复完整 8 帧，受影响路径已独立验证 |

**最终表述**：**同意“发布链 PASS / M6 PENDING”**。发布链可写 PASS（有溯源、有干净输出树、有 e2e verdict），但 M6 里程碑不因此关闭：科学比较三项、双平台、干净安装/恢复、性能/故障报告仍未完成。P0 无；P1 无（上轮两项均已关闭/降级）；P2 persistent 为 CAVEAT，另有三项 P2/P3 文档/库存项。

**追记签署**：reviewer-m0m4。仅记录复核证据与建议，未改仓库代码/用例，未 commit/push。
