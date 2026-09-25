# ACSD 交接文档（给下一个对话）

接手前先读本文全篇，再按 §1 的权威链读文档。本文只写「新对话不知道就会踩坑」的事。

## 0 交接时的状态

- 仓库：`/workspace/Astro CS Database`（**路径含空格，命令一律加引号**）
- **冻结基线 = 本文档所在的提交**，用这条命令取它（自指，不会随新提交而失效）：
  ```bash
  git log -1 --format=%H -- eng/tools/HANDOVER.md
  ```
  基线在冻结时：构建 `ninja -C build` rc=0、`validate_registry.py --strict` 0 error（150 条注册项）、
  工作树干净、`HEAD == origin/main`。
- **本对话已结束。负责人的指令是：保持基线静止**，不做测试、不做端到端、不做全量构建。
- **当前阶段 = 只读推导找错**（见 §4）：扫描由负责人在只读节点做，产出报告；
  开发节点接报告、逐条修 bug。**全部修完且找不出新问题之后**，才做彻底构建与实测。
- 待修清单的正本是外部审查节点维护的**一页纸**，见 §5。

## 1 这是什么项目、先读什么

ACSD = Astro Celestial Sphere Database，天文 CCD/CMOS 图像校准与标准化数据库。
三个**独立**命令：`normalize`（单帧标准化）→ `mosaic`（马赛克）→ `export`（投影导出）。
阶段间只通过磁盘产品 + manifest + 哈希交换。正式平台 Windows x64 与 Linux amd64，纯 CPU 生产。

**权威链（逐层下钻，不是挑一篇读）**：
`ASTROCS_DESIGN.md`（最高，先通读相关章节）→ `docs/design/` → `docs/plugins/` →
`docs/science/` 与 `docs/algorithms/`（公式与推导，只读权威）→ `docs/contracts/` →
`ENGINEERING_SPEC.md` → `ACCEPTANCE_SPEC.md` → `docs/ci/` → `docs/research/`。

**机器契约字面量冻结、改名时绝不可动**：`x-astrocs-*` schema 键前缀、模块 ID `astrocs.*`、
`ASTROCS_*` 环境变量与 CMake 选项、`namespace astrocs`、`#include <astrocs/...>`、
目录 `lib/include/astrocs/**`、文件名 `ASTROCS_DESIGN.md`。
显示名是 `ACSD` / `Astro Celestial Sphere Database`；判定规则（唯一源在 `ENGINEERING_SPEC.md`）：
**把该处换成 ACSD 后是否有任何机器会失配或指向不存在对象——会则保留原名，否则必须写成 ACSD**。

## 2 环境事实（新对话最容易误判的）

- 机器：**23.5 GiB RAM，swap 永久禁用**；16 逻辑核；`/workspace` 503 GiB；容器内，**`dmesg`/`journalctl` 为空**，无内核日志。
- **内存看门狗**：一切重计算必须经
  `python3 eng/tools/monitoring/mem_guard.py --max-rss-gb <N> --timeout <秒> -- <命令>`；
  它按**进程树** RSS 采样，超限只杀该命令的进程组（rc 137），不触及 DSH 本体。
  **上限 = 正常峰值的 1.5–2 倍**，不是「物理内存的几分之一」。实测单帧 `acsd normalize` 峰值 **15.7 GB**、
  三阶段端到端 normalize/mosaic/export 峰值 **15.697 / 6.986 / 0.328 GB** ⇒ **端到端取 19–20 GB**，
  单进程探针 4–8 GB。**超时用 `--timeout`，不要套外层 `timeout(1)`**（外层只杀看门狗本身，子进程会逃逸成孤儿）。
- **DSH 自身会死，且不是内核 OOM**：23.5 GB 机器上 OOM killer 按最大 RSS 挑选，那是 `acsd`（十余 GB），
  不是 DSH（约 1 GB）。DSH 的死因是 **Node V8 堆耗尽**（无 `--max-old-space-size`），
  **由巨大的工具输出 + 高并发子代理驱动** ⇒ 保持工具输出短、子代理并发 ≤ 4–6。
- **会话被杀的机制**：长前台工具调用撞 harness 墙钟上限（约 600 s）会连带杀掉后台作业；
  **前台 sleep 必须 ≤ 约 360 s**。DSH 重启后长期目标**按设计自动解除武装**，只有人类发话才重新武装。
- **`/tmp` 会被清空**：脚本放 `run/<轮次>/`，不要放 `/tmp`。**`run/` 是 gitignore 的，不入库。**
- **`setsid`/`systemd-run` 都不可用**（前者进程活不下来，后者 Access denied）⇒ 后台作业只用 harness 的
  `run_in_background: true`。
- **盘满不报盘满**：表现为子代理莫名失败、`OSError: [Errno 28]`、产物半截，极易误判成代码 bug。
  **重计算前先 `df -h /workspace`**；低于约 100 GiB 先回收。回收动作：删已登记轮次内部的 `run/<轮次>/out/`
  产品树（报告/证据/results/logs 保留）。**本轮实测：`run/M42-E2E-02/out` 一个目录就占 167 GB。**
- 审批提示在本会话**已禁用**：不要设 `sandbox_permissions`，被拒即终局。

## 3 硬禁令（违反即回退）

不动科学公式/默认容差/SCI·ALG 冻结定义（除非走变更流程）；不串三阶段；不硬编码线程/ISA/block；
不在 main 外开分支；**不 force push / amend / 历史重写**；产物不落仓库根（落 `output_dir` 或 `run/`）；
不宣布发布；不用 facade/空骨架冒充实现；不以「环境问题」掩盖失败；**不用 waiver 盖红灯**；
**禁止放松判据或用删断言/降阈值把门弄绿**；不打印密钥（Fatduck 只用 `-i` 引用）。

## 4 测试策略（**负责人本轮明确定的口径，务必遵守**）

负责人原话：「**为什么你总是在做很重的测试，我认为没有意义。除了端到端测试没必要做这些了，**
**一个模块确认没问题后可以冻结，在下次更改或者影响到他之前不需要反复测试。**」

### 4.1 当前阶段：**一切测试与端到端暂停**（负责人最新裁决，优先级最高）

负责人原话（本对话结束时下达）：

> 「先停止端到端。下一个阶段应该是对仓库进行**大规模静态检查**（这个我在只读节点做好，
> 形成报告，然后拿回本机在下一个对话里修复 bug）。**全部完成前不要再做任何测试与端到端**，
> 那些没有意义，根本检查不出来问题，**必须靠只读推导来找错误**。
> 全部修完找不出来新的问题再彻底构建和跑实测。」

因此：
1. **不要跑构建、不要跑 ctest、不要跑 `run_checks.py`、不要跑三个命令的端到端。**
2. **找错的唯一手段是静态检查与推导**（读代码、读文档、沿权威链核对、写工单）。
3. 扫描由负责人在只读节点做；开发节点**接报告、逐条修 bug**，每条必须带权威锚。
4. **全部修完且找不出新问题之后**，才做彻底构建与实测。

**为什么「跑测试找不出问题」**——这不是感觉，是实测过的机制：测试判据与缺陷**正交**。
本仓最典型的一例：直写路径漏乘归一分母，默认 `pixfrac=0.8` 下**生产亮度整体偏亮约 0.48 星等**，
而测试面 `pixfrac` 取值**零处小于 1**（40 处全是 `1.0`/`1`）⇒ 没有任何现存判据会因分母取错变红。
同类空档在本仓不止一处，扫漏洞时优先找「判据断言的对象与缺陷所在的对象不是同一个」。

### 4.2 解禁之后才适用的运行节奏（先别用，留作解禁后的口径）

1. **默认按影响面增量运行**：`python3 eng/ci/run_checks.py`（**不带参数就是 `--changed`**，见 `docs/ci/CI_SPEC.md` §2.1）。
   全量必须显式 `--all`。
   ⚠️ **已知退化**：改动集命中 `eng/ci/**` 这类全局敏感面时，增量档会 `escalate→full`（实测本仓多次如此），
   此时它会跑 190 个 step。看到 `scope=full` 就别跑，按需 `--check <ID>` 点名。
2. **ctest 只跑改动所及的目标**（`ctest -R <受影响目标>`），**不跑全量**。
3. **端到端独占机器**——不得与任何其他重活并发，包括子代理的。
4. **模块确认后冻结**：在它被改动或受影响之前不重测。

负责人另一条相关判断：「**事实证明门禁并不可靠，而且反复失效**」「**现在应该注重正向解决问题**」——
意思是**优先修产品真缺陷**，不要陷入「加门—门坏—修门」的循环。

## 5 待修清单的正本：一页纸（在外部审查节点上）

**位置**：`F:\Astro dev\独立审查\整改\一页纸.md`（Windows 节点 Fatduck 上，**不在本仓**）。
**本仓副本**：`run/ONEPAGER/onepager_latest.md`（同步方法见下）。

**它由审查节点维护，开发节点不改它**；开发节点维护的是状态件
`F:\Astro dev\独立审查\整改\开发节点-整改状态.md`（本仓副本 `run/ONEPAGER/STATUS.md`）。

**访问路径**：`agent`（本机）→ `vm-bj`（`100.73.70.16`，root）→ `Fatduck`（`100.104.10.71`，用户 `fujia`，pwsh 7.6.3）。
两个现成脚本：
- `eng/tools/fatduck_ps.sh '<PowerShell 命令>'` —— 在 Windows 节点执行一条命令；
- `eng/tools/fatduck_put.sh <本地文件> '<Windows 绝对路径>'` —— **分块**写文件（整文件 base64 会超命令行长度上限，实测 9 KB 即被拒）。

**同步一页纸**：
```bash
./eng/tools/fatduck_ps.sh '[Console]::OutputEncoding=[Text.Encoding]::UTF8; Get-Content -Raw -Encoding UTF8 -LiteralPath "F:\\Astro dev\\独立审查\\整改\\一页纸.md"' > run/ONEPAGER/onepager_latest.md
```
（先看字节数与 md5 是否变了——审查节点更新很频繁。**交接时是 42533 字节、S1 已从 17 条涨到 29 条。**）

**GitHub 只读检查节点**依据 GitHub 检查 ⇒ **要及时 push**，push 后 `git fetch` 核对 `HEAD == main == origin/main` 三 SHA 一致。

### 5.1 报告到手后先做锚核验（`eng/tools/audit_intake.py`）

**审查报告的引用锚会失真**，实测过的三种形态：
① 引用的文件**只存在于临时快照**（`run/*/wsrc/`，`run/` 不入库，当前树里根本没有）；
② 行号随并发改动**漂移**，指向别的代码；
③ 文件与行号都在，但该行内容与该条声称的**不是一回事**。
先做锚核验再动手，可以避免整条工单追逐一个不存在的对象。

```bash
# 只读：不跑构建、不跑测试、不写仓库
python3 eng/tools/audit_intake.py <报告文件.md|.csv> --out run/ONEPAGER/worklist.csv
```
出口：`0` 全部锚可核 / `1` 存在失效锚（逐条标出）/ `2` 输入缺失或不可解析（fail-closed）。
每条锚判为 `OK` / `TEMP_ONLY`（只在临时快照里）/ `FILE_MISSING` / `LINE_OUT_OF_RANGE` /
`RANGE_OUT_OF_RANGE` / `NO_ANCHOR`（条目根本没有可核验的 `文件:行`）。

**注意**：`NO_ANCHOR` 与 `TEMP_ONLY` 都要**在动手前**向负责人确认，不要直接开修——
它们可能意味着「该条在冻结基线上不成立」或「该条引的是已被回收的旧快照」。

## 6 S1 二十九条的状态（交接时的快照）

### 已修
| # | 条目 | 提交 / 证据 |
|---|---|---|
| 2 | 生成器与判定脚本把结论写死 | `23614142`、`5f8c237b` |
| 4 | provenance 通量折算键无数值断言 | `822b9c53` |
| 6 | 第三方源清单生成器指着已迁走的旧目录 | `23614142` |
| 13 | 验收判据与设计冻结语义相反 | 前提被证伪；`23614142` 补口径一致性门 |
| 14 | 接缝门限 `1e-2` 无推导 | 正本在 `docs/science/PHASE2_UPM.md` §17 |
| 16 | 最高设计星表规模数字无来源 | 前提被证伪；权威下沉到 schema/登记面 |
| 17 | 项目改名未贯穿 | `23614142`（164 处 + 保留面门） |
| 21 | 改名把头路径写成不存在、门恒红 | `7a59b946` |
| 22 | 校验器先覆写被校对象再比对 | `5f8c237b` |
| 25 | 「文档提到字符串」当成构建图校验 | `5f8c237b`（新 `cmake_graph.py`） |
| 26 | 不变量断言与命题反向 | `5f8c237b` |
| 29 | 生产入口零登记且缺失被写成断言 | `5f8c237b` |
| 24 | 缺件即 skip 让整类校验归零 | `5f8c237b`（声明输入面机制） |
| B 组 | `DELIVERED` 造词 / `waivers.json` 悬空 / 分母 713 无定义 / 快照失配 | `5f8c237b` |
| C 组 | ISA 声明与编译不同源、门只读根 CMake | `5f8c237b` |
| E 组 | 内存结论由构造恒真、两个并存分母 | `5f8c237b` |
| F 组 | 产品过不了自己的合同 | `5f8c237b`（新门）；**产品本身仍有 153 条问题** |
| G 组 | 引用状态自相矛盾 | `5f8c237b`（两门；**5 + 6 条真实红灯未修**） |
| 18 | `pixfrac<1` 直写路径 signal 偏 `1/pf²`、variance 偏 `1/pf⁴` | **已修，见 §7——这条改变了生产输出** |
| 19 | 球面重叠静默吞掉面积失效 | 已修（三处改具名失败 + 计数进 provenance） |
| 20 | 被注释的红灯与 `WILL_FAIL` 负例门 | **部分**：三态合同与豁免分支负例已做；`WILL_FAIL` 区分「预期失败/没跑成」未做 |
| 1 | 全量扫描非绿 | **部分**：`crash` 1→0；内容真红仍有约 30 条 |

### 未动
`7` 验收证据链失真 · `8` 科学验证由构造恒真 · `9` 对外结论来自未接线路径 · `10` 逐帧配对不变式无承载 ·
`11` 第二套构建并存（8 个模块 Makefile 无任何检查项覆盖） · `12` 跟踪面有进不了构建的测试源 ·
`15` schema 强制四层复述 · `23` 并行一致性对照臂走生产不消费的键 · `27` 两处对外承诺与实现相反 ·
`28` 下层把「两轴之积」写成「之和」 · `3`/`5` 待负责人裁口径（见下）

### 待负责人裁（定前不动代码）
- **3 通量折算正本**：本节点已按「面积归一 ⇒ 因子恒为 1、与 pixfrac 无关」统一，并写成不变量；§7 的修复沿此口径。
  若负责人裁定另一条，需连带改回。
- **5 精度键正本**：设计用位深 / 合同用 `config.precision` / 实现消费 `bitpix` ⇒ 需指定唯一正本。
- **15**：`hips_storage_form.schema.json` 的 `must_contain` 要求每层字面含同一组口径词，与「口径唯一源」互斥。

## 7 ⚠️ 端到端证据已失效，必须重跑

`M42-E2E-03` 在**修复前**的代码上跑通（三段 RC 全 0，峰值 15.697 / 6.986 / 0.328 GB），
但它用的配置是 **`pixfrac: 0.8`**，而 §6 的**第 18 条修复改变了生产输出**：
`hips_profile=0` 直写路径原先漏乘 `k = D_p/N_p = pixfrac²` ⇒ 默认 `pixfrac=0.8` 下
**signal 偏大 56.25%、variance 偏大 144.14%**。修复后 signal 缩小 1.5625×、variance 缩小 2.4414×。

**量级要说清：默认 `drizzle.pixfrac=0.8` 下生产亮度整体偏亮约 0.48 星等**——不是舍入误差，是产品级偏差。
零回归已实测：`pf=1` 时 `pixel_area ≡ drop_area` 是同一个变量 ⇒ `k≡1.0` 逐位，既有 `pf=1.0` 用例逐位不变。

**它此前为什么测不到**：测试面 `pixfrac` 取值**零处 <1**（40 处全是 `1.0`/`1`）——端到端夹具、CLI 集成、
节点测试统一钉住 `1.0`；唯一跑 pf<1 的验收 B 段判的是累加器侧 `Σ_p F_p = Σ_j x_j` 与 FP64 闭合，
二者按构造与归一化无关 ⇒ **没有任何现存判据会因分母取错变红**。这类「判据与缺陷正交」的空档本仓不止一处，
新对话扫漏洞时优先找它。

⇒ **结论：现有 M42 产品不能作为证据。但也先不要跑**——当前阶段禁止任何测试与端到端（见 §4.1）。

**留到「全部修完且找不出新问题」之后，作为解禁后的第一次实测**：
`bash run/M42-E2E-03/driver.sh`（约 90 分钟），驱动脚本与日志在 `run/M42-E2E-03/`，
自带 `mem_guard --max-rss-gb 19 --timeout 21600`。
**跑之前确认没有别的重活在跑，且 `df -h /workspace` 有 200 GiB 以上。**

注意：端到端夹具显式钉住 `pixfrac: 1.0`（`eng/tools/e2e/make_e2e_configs.py:66`），
而 M42 的配置是 **0.8** ⇒ 两者口径不同，新对话要把这个不一致本身也当成待查项。

## 8 本轮新暴露的真缺陷（门修好后浮出来的）

1. **51/51 个 M42 落盘产品不携带任何交换对象文档**（无 `product_role`/`type_id`/`artifact_manifest`/`product_content`）
   —— 不是少一两个键，是**缺整个合同面**；平面语义只能靠目录名与 FITS EXTNAME 推得。
2. **幂次用一个标量描述不了全部语义面**：`pixel_area_power=-2` 与 `variance_bunit="ADU^2/sr^2"`(−4)、
   `ivar_bunit="sr^2/ADU^2"`(+4) 并存；落盘 FITS 印证 −2/−4/+4。
3. **引用状态自相矛盾 5 条、越界引用 6 条**（含 `costa 1992`、`merline 1995`）；
   注意 `Merline & Howell 1995` 的 **DOI 与卷页经 Crossref 对得上——问题在未核实，不在错引**。
4. **364 个 step 里 314 个的扫描对象完全在脚本内部，注册面根本无从校验**——这是机制性缺口。
5. **`CHK-PROD-WIRING` 注册了 45 个 ctest 目标而实现从不调用 ctest**（已修，49→0）。
6. **`p3_rejection.bin` 载体不存在**，既存未登记门 `check_p3_rejection_count.py` 返回 rc=2。
7. **8 个模块 Makefile 是无任何检查项覆盖的独立构建路径**（`-march=native` 已移除，是否收敛到根构建图待裁）。
8. **`CHK-DOCS-MACHINE-CONSISTENCY` 的锚密度棘轮与机器登记面冲突**（0.9623→0.9715）：
   机器登记表每行恰 1 个锚，与散文锚密度度量不是一回事；抬基线＝放松棘轮、排除＝改判据，两者都需授权。
9. **`CON-BUILD-GRAPH` 的 `changed_paths` 不含 `CMakeLists.txt`** ⇒ 根构建图变更不触发该门。

## 9 我（上一个对话）犯过的错，新对话不要重犯

1. **从过窄的证据面下全局结论**（三次）：断言「内核 OOM 杀了 DSH」（实际按最大 RSS 挑选的是 `acsd`）、
   断言「三件事一件都没在生产里跑」（实际预算与压力感知是通的）、断言「规范层根本没有这条要求」（实际在 §8.3 写着）。
   ⇒ **下结论前先把检索词换两三套，并让独立子代理复核。**
2. **并发跑重活**：一边 `ctest -j 4`（四个 2.5 GB 的验收测试并排）一边开着子代理，把机器压到 4.5 GB 可用内存。
   ⇒ **重测试独占机器。**
3. **反复跑全量**：把「验证完备」误解成「把所有东西再跑一遍」，而不是「只验证这一轮改动的影响面」。
4. **误判「查不到」为「不存在」**：`grep 'lib/include/acsd'` 返回 0 命中，因为那段路径由
   `os.path.join(REPO, "lib", "include", "acsd", ...)` 的**多个字面量拼成、整串检索不到**。
   ⇒ **被拆散的字符串要换搜法（搜单个字面量）。**
5. **`/tmp` 放脚本**：会话重启后 `/tmp` 被清空，作业 exit 127。
6. **JS 模板字面量里的反引号/`${}`**：会截断字符串或触发语法错误（`${#VAR}` 被当成私有字段）。
   规避：`String.fromCharCode(96)`、`printf %s "$VAR" | wc -c`。

## 10 立过的科学/工程口径（新对话别推翻，要改先走变更流程）

- **通量折算**：drop-area 归一（`Σout = Σin`，与 pixfrac 无关），`flux_conservation_factor ≡ 1`；
  修复后归一分母统一到唯一源 `drizzle::sb_publish_scale`（`astro_sphere_sink.h:50`）。
- **排异路由（M3）**：`1≤n≤3 none / n=4..5 percentile / n≥6 winsorized_sigma`。
- **内存治理三层**：CLI 解析预算（默认 95%，`host.memory_budget_percent` 可覆盖）→ 运行时资源预算 →
  调度器预留式回压（`mem_used+need<=limit`，否则 park）。
- **`ASTROCS_DESIGN.md` §8.3 编排策略六条**是内存治理的正本；**下游不得复述**。
  其中「可丢弃重跑」这条**仍未接线**（实现语义只存在于未跟踪草稿）。
- **设计条文接线台账**：`eng/ci/ledgers/design_clauses.json`，41 条 = 23 已接线 / 18 未接线。
  未接线主因四类：① 零实现；② **配置键静默失效**（`§5.3 snr_path`、`§5.5 algorithm_rejection_method`
  在 `dead_config_keys.json` 里——**用户选了但什么都不发生**）；③ 只在验证/流程面；
  ④ **代码不在生产构建图里**（`lib/algorithms/projection/p3_projection.cpp`：该目录 CMakeLists 只编译 `p3_wcs.cpp`）。

## 11 常用复现命令

```bash
cd "/workspace/Astro CS Database"

ninja -C build -j 4                                        # 构建
python3 eng/ci/run_checks.py                               # 影响面增量（默认；注意 scope=full 退化）
python3 eng/ci/run_checks.py --check <ID> --quiet          # 点名跑，重测试用这个
python3 eng/ci/validate_registry.py --registry eng/ci/checks.json --strict
python3 eng/tools/doccheck/check_doc_hygiene.py            # 文档卫生

# 重计算一律套看门狗
python3 eng/tools/monitoring/mem_guard.py --max-rss-gb 19 --timeout 21600 -- <命令>

# 端到端（约 90 分钟，独占机器）
bash run/M42-E2E-03/driver.sh
```

## 12 谁写什么

| 角色 | 权限 |
|---|---|
| 前台（你） | **只有前台能 git 写**；能写 `checks.json` / `id_migration_map.json` / `prod_wiring_baseline.json` / `known_failures_baseline.json`；统一登记新检查项的 ID 迁移 |
| 子代理 | **零 git 写权限**；只做单元工作；报告必须含「我证伪了任务书的哪个前提」 |
| 审查节点 | 维护一页纸，**只读**；不改仓库 |

**新增检查项的流程**：写门 → 登记进 `checks.json`（聚合项 `command` 必须写成
`['python3','eng/ci/run_checks.py','--check','<ID>','--quiet']`，否则 R12 红）→
**在 `id_migration_map.json` 的 `mappings` 加 `old_id`、在 `targets` 加注册项**（R13 要求每个执行单元都登记，
**没有第三个文件能替代**；`CHK-KNOWN-FAILURES-BASELINE` 必须排在档末，否则 R10 红）。

**提交纪律**：一个 commit = 一个明确目的；**科学/架构/性能/文档不混提**；
提交消息写「做了什么 + 依据哪条权威条款」，**不写任务编号长串、不写流水账**。