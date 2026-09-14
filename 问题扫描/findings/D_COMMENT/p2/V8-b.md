# V8 片2-3 · P2/P3 与计量

### V8-N-06（P2·判据②）`gaia_client.h` 公共头契约锚指向**三个不存在的节名**
- `:3`（`bd4bc23b` 新增整行）「契约见 `docs/algorithms/GAIA_QUERY.md` **Postconditions/Invariants/并行模型** 与 `docs/architecture/CACHE_POLICY.md` Gaia查询缓存行」。
- 复算：`GAIA_QUERY.md` 实为 `:191 ## 4. 并发模型（现状，与源码一致）`、`:216 - **不变量（properties）**`；**"Postconditions" 0 命中、"并行模型" 0 命中**（**"并行"与"并发"在合同语义上不等价**）。`CACHE_POLICY.md` 的 Gaia 缓存行 **命中 ⇒ 该半为真**。
- **同句其余自报量复算全真（防误伤）**：64 条、`64×200000×3×8 = 307,200,000 B`（与 `module_entry.c:546 kQueryCacheCap` 同式）、TTL 60、8192 槽=2^13、4GiB 总预算、`count/4+1` 淘汰、事务式替换（`gaia_client.c:732`）。⇒ 只判三个节名。
- 后果：公共头是 C ABI 消费入口，按注释给的节名检索**必失败**。**related** `V6-N-07`（snr 行锚 9/9 漂移）、`V8-N-04`、`V12-N-11`（同 307,200,000 常数值此处为真，不重报）

### V8-N-08（P3·判据②）角度归一化契约**区间端点写错**：头注释 `(-90, 90]` 左开，而 `-90` 是可达返回值
- `lib/star_detector/src/sdet_angle_guard.h:38-39`「归一化到 `(-90, 90]`」vs `:56-63` `while (std::fabs(angle_deg) > 90.0)` ⇒ 输入恰 `-90` 不进循环、**返回 -90（可达却被文本排除）**；测试 `sdet_angle_guard_test.cpp:96-104` 断 `fabs(out) <= 90.0`「归一化结果落在 `[-90, 90]`」⇒ **头注释左开、测试闭区间，两处口径不一**。
- **同文件复算为真的部分**：`:34`「64 次（≥ 11610°）」（`90+180×64=11610` 恰接受、11790 拒）、`:24`「有限值逐位不变」⇒ **只登记端点一处**。改法：文本统一为 `[-90, 90]` 或与实现一致的 `(−180,180]` 语义说明。**related** `V12-N-08(c)`（边界开闭同族）、`S-1` 第 4 条

## 计量交付（**非缺陷，不入账本**）
### `scripts_v8_runrefs.py`：本批**新增行内** `run/` 引用 = 19 处 / 15 文件，扣 2 处假阳性 ⇒ **真引用 17 处 / 14 文件**（HEAD `521095b8`）
- 假阳性：`docs/traceability/TRACEABILITY_MATRIX.json:448/:760` 的 `"run/inspect/destroy"` 实为 `p2_session_create/validate/run/inspect/destroy` **符号列举**。宿主集中于 **P1-gaia / P2-psf / P4-magiter / P5-snr / P8-snr-linux / release-rescue** 六族（清单在 `_verify/V8.md §5`）。
- **口径区分（必须写清，防被当成与我的数打架）**：前台 `56b5e662` 的「573 处 / 194 文件」是**全仓 `run/` 全引用**（我的 `_front_runref_classify.py` 分类 A=59 危险 / B=65 合法声明 / C=437 中性未判）；本条是**本批新增行 × 5 路范围 × 注释面**，**两者不可相减**。
- 盘上可看、仓内无物（与 `V6-N-04` 同判，不另立条）：`P4-magiter/REPORT.md`、`P5-snr/harness/snr_oracle.py`、`release-rescue/**/PHOTOMETRY_LITERATURE_REVIEW.md` 本机在盘，而 `git ls-files run/` **只有 `run/.gitkeep`**。

## 反向取证：**P9 合同实测数字静态复算全部为真**（给隔壁的通过项，防我一味报坏）
- `scripts_v8_p9_frames.py` 只读解析 `testdata` 下 **906 个 `.fts`** 的 FITS 头：`scanned 906 err 0`；缺 `FOCALLEN`/`XPIXSZ`/`OBJCTRA`/`OBJCTDEC` 任一项 = **0 帧**；**无 `CRVAL1/2` = 31 帧**，分布 `{Galaxy_Center_T4:1, M42_T2T3_mosaic:30}`。
- `scripts_v8_p9_dev2.py`：`max |CRVAL−OBJCT| = 0.88247°`，落在 `NGC55_T3 …075716-1200S-H-alpha.fts`；`scripts_v8_p9_idx.py`：该帧"全库路径升序"下标 **561**（数据集内 34、数据集序 365）；在册 `testdata/index.json` 的 `Σlights_count = 906`（盘上实数亦 906）。
- ⇒ **`DATA_SEMANTICS.md §18.5:806-809` 的四项计数与 `lib/core/README.md:19-22` 的 `206.265` 量纲说明（含 `ipv_select.cpp:57` 行锚）全部复算为真。**唯一建议：合同用「idx561」当标识却**未写排序口径**（换一种遍历顺序就不是 561）⇒ 属改进，不计缺陷。
- **注意这两条合起来读**：P9 的**数字**为真（本节），但 P9「帧头 WCS 未授权」的**裁决登记**在登记面 0 条目（`V8-N-07`）⇒ 二者不矛盾：**测得准不等于有权改**。
