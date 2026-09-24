# Astro Celestial Sphere Database（ACSD） — 天文 CCD/CMOS 图像校准与标准化数据库

ACSD 把单帧观测转换为可独立消费、带不确定度与来源链的球面科学产品（HiPS），再按明确科学目标
合成马赛克、导出测量意义明确的 WCS FITS（`ASTROCS_DESIGN.md` §1.1）。

## 权威链与索引

权威链只有一条，权威程度自上而下递减（`ASTROCS_DESIGN.md` §0.1）；每一层由它的上一层推出，
下级文档陈述与本设计一致的细化内容。与本文档集冲突时以 `ASTROCS_DESIGN.md` 为准（该文 §0）。

| 序 | 入口 | 回答什么 |
|---|---|---|
| ① | [`ASTROCS_DESIGN.md`](ASTROCS_DESIGN.md) | 是什么、做到什么、顶层架构、CLI、发行、验收（最高设计） |
| ② | [`AGENTS.md`](AGENTS.md) | 机器干活手册：下钻顺序、工作流、纪律 |
| ③ | [`ENGINEERING_SPEC.md`](ENGINEERING_SPEC.md) | 代码、测试、提交、目录与 CI 规则 |
| ④ | [`CONTROL_PACK_SPEC.md`](CONTROL_PACK_SPEC.md) | 控制包的制作与执行规范 |
| ⑤ | [`ACCEPTANCE_SPEC.md`](ACCEPTANCE_SPEC.md) | 四层验收标准与预览版发布门 |
| ⑥ | [`docs/ci/CI_SPEC.md`](docs/ci/CI_SPEC.md) | 机器门怎么跑、证据落哪 |
| ⑦ | [`docs/plugins/00_INDEX.md`](docs/plugins/00_INDEX.md) | 逐模块工作细节 |

与上述入口并列的下级权威：`docs/science/`（科学公式与定义式）、`docs/algorithms/`（算法推导与符号表）、
`docs/design/UNIFIED_MODEL.md`（数据对象与三类配置）、`docs/contracts/`（合同的文档化说明，与 `eng/contracts/`
的机器校验 schema 双向对应）。全文档集的唯一索引地图 = `docs/DOCUMENT_INDEX.yaml`；文档体系分层见
`docs/standards/DOCUMENTATION_STANDARD.md`。

## 三个命令，三个独立产品

`normalize` / `mosaic` / `export` 是三个平级独立命令：各自独立启动、独立重跑、独立验收，阶段间只通过
磁盘产品、manifest 与哈希交换（`ASTROCS_DESIGN.md` §1.2；产品交换合同见
`docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md`）。对外只有一个 CLI 入口 `acsd`，它按命令
拉起对应阶段的调度器（§8.1）。`phase` 是设计与目录层面的内部指代，命令名与代码目录用
`normalize`/`mosaic`/`export`（§7.1）。

| 命令 | 内部指代 | 输入 | 输出 |
|---|---|---|---|
| `normalize` | Phase1 | JSON 配置（数据块 = 一组 light + 对应校准帧 + 滤镜） | 标准化单帧 HiPS（帧级 SNR 入文件头，可选稀疏帧内 SNR 层）+ 结构化 JSON |
| `mosaic` | Phase2 | 一组合同兼容 HiPS + JSON 配置 | 马赛克 HiPS + UPM/排异/集成 provenance + 结构化 JSON |
| `export` | Phase3 | 任一合同兼容 HiPS + JSON 配置 | 平面 WCS FITS + 结构化 JSON |

三个命令的 JSON 模板由 `--template` 生成，字段说明由 `--help` 给出（`docs/api/CLI_PROTOCOL_V1.md`）。
正式平台为 Windows x64（交付 `acsd.exe` 与各 `.dll`）与 Linux amd64（`acsd`），纯 CPU 生产；
ACR 源码保留为隔离实验，生产构建路径不含它（§1.3、§8、§10）。

## 科学目标

科学核心是一条链：先把每帧校准到统一的测光星等坐标系，再在这个坐标系上测量不受天光影响的绝对信噪比，
最后用加性方式去除天光、建立帧间连续的绝对信号平面，使叠加结果天然无接缝（§2）。三个创新点互为前提，
各自以独立实验单元呈现于 `实验/`，alpha 发布前全部经实验与独立审稿证实（§2、§12.3）。

1. **测光校准到测光星等坐标系**（§2.1）：只对星点测光，星点位置由 Gaia DR3 XP 星表逆映射到本帧像素域获得；
   用 Gaia DR3 XP 星点光谱 × CCD QE 曲线 × 滤镜透过率曲线积分正向合成期望测光量，与实测通量拟合得到逐帧
   线性乘性标度 `k_photo`（`I_photo = k_photo·I_cal`），把整帧对齐到统一相对测光零点并消除物理单位。
   公式、单位与适用域见 `docs/science/PHOTOMETRY.md`。
2. **跨帧可用的绝对信噪比**（§2.2）：交付物是 Phase1 实际产出的 PSF 信号 SNR（逐源 `SNR_F = F/σ_F`），
   以帧级标量 `frame_snr` 与帧内稀疏控制点层 `sparse_snr_layer` 两个对象承载；Phase2 由它重建稠密 SNR 场
   并定权 `w(x,y) = SNR(x,y)²/F_ref² = 1/σ_F(x,y)²`。定义与推导见 `docs/science/NOISE_MODEL.md`、
   `docs/science/CONTROL_WEIGHT_SNR.md`。
3. **加性天光与无接缝叠加**（§2.3）：UPM 在全部帧上联合建立一张连续的绝对天光参考平面，每帧按
   「多退少补」加性扣除自身偏差并保留公共天光，帧集变化处结果连续。定义见 `docs/science/PHASE2_UPM.md`。
4. **全天 HEALPix 产品的精确面积交叠分配**（§2.4）：以 12 个基面的等面积 chart 为坐标域（Jacobian 恒为 π/3），
   drop 足迹映射进 chart 后交叠退化为轴对齐裁剪 + 鞋带公式，逐 leaf 面积按绝对立体角口径累加。
   几何与判据见 `docs/algorithms/DRIZZLE_GEOMETRY.md`。

## 构建与运行

构建需要 CMake + Ninja 与 C++ 工具链；依赖与工具链冻结值见 `DEPENDENCIES.md`（Windows preset 合同 =
根 `CMakePresets.json`）。

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # 仓库唯一的根 CMake
ninja -C build
ctest --test-dir build --output-on-failure
python3 eng/ci/run_checks.py                              # 机器一致性检查（注册表 eng/ci/checks.json）
```

```bash
acsd normalize --json <config.json>    # 单帧标准化
acsd mosaic    --json <config.json>    # 马赛克
acsd export    --json <config.json>    # 投影导出
acsd help / acsd --version / acsd doctor / acsd benchmark
```

命令树是唯一命令面（§7.1）；配置结构、事件流与退出码见 §7.2，日志与错误传播见 §7.3 与
`docs/contracts/LOG_AND_ERROR_CONTRACT.md`。机器门注册表 = `eng/ci/checks.json`，运行方式与判据见 `docs/ci/`。

## 仓库布局

模块索引权威 = `docs/architecture/MODULE_MAP.md` 与 `docs/modules/`；根目录固定条目与新增约定见
`ENGINEERING_SPEC.md` §7。

顶层分四块：算法在 `lib/algorithms/`（并联放置），基建与 CLI 在 `lib/infrastructure/`，
工程支撑面在 `eng/`（合同 schema、机器门、构建与质量工具、程序全局配置），自解释文档集在 `docs/`。
科学实验单元在 `实验/`，证据在 `artifacts/`，控制包工作区在 `工程控制/`，
开发/CI 过程产物与过程日志在 `run/`（不入库；回收机制见 `eng/tools/run_gc.py`）。

根目录固定条目、各目录的完整职责与新增约定见 `ENGINEERING_SPEC.md` §7；
模块清单见 `docs/architecture/MODULE_MAP.md` 与 `docs/modules/`。

## 状态口径

状态词表唯一口径 = `ASTROCS_DESIGN.md` §12.5：`CONTRACT_READY` / `IMPLEMENTED` / `INSTALLED` / `VERIFIED`，
负向 `NOT_IMPLEMENTED` / `NOT_VERIFIED` / `DEFERRED` / `DORMANT` / `FAIL`；状态由检查与验收现场计算，
登记表不预写状态。

- 逐模块/逐阶段状态与证据锚：`docs/owner/RELEASE_STATUS.md`、`docs/modules/MODULE_MAP.yaml`。
- 发布口径：`VERIFIED` 要求正式平台（Windows x64）与真实数据验收通过；agent 至多声明
  `READY_FOR_OWNER_REVIEW`，最终发布决定由项目负责人作出（§12.5、§13）。
- 版本口径：产品版本唯一事实源 = 仓库根 `VERSION`（形态 `MAJOR.MINOR.PATCH-alpha.N`），CMake、CLI、
  产品 manifest 与活动文档由该源派生（单源条款 = `docs/owner/RELEASE_STATUS.md` §2）。
- 待决条款的计数与逐条登记：`eng/contracts/data/v6_clause_registry_v1.json` 的 `counts` 段
  （语义权威 = `docs/contracts/DATA_SEMANTICS.md` §31.10）。
- 已知限制台账：`docs/KNOWN_LIMITATIONS.md`。

## 细节往哪读

| 要读什么 | 去哪读 |
|---|---|
| 数据对象、三类配置、逐阶段详细设计 | `docs/design/`（入口 `docs/design/UNIFIED_MODEL.md`） |
| 科学定义式、单位、适用域 | `docs/science/` |
| 算法推导、符号表、算法级边界 | `docs/algorithms/` |
| 产品字段、键集、值域（文档侧） | `docs/contracts/` |
| 机器校验的 schema（机器侧唯一事实源） | `eng/contracts/` |
| 模块工作细节 | `docs/plugins/`、`docs/modules/` |
| 架构与不变量 | `docs/architecture/` |
| 跨阶段产品交换与 ABI | `docs/interfaces/` |
| CLI/API 协议 | `docs/api/CLI_PROTOCOL_V1.md` |
| 机器门清单与运行方式 | `docs/ci/` |
| 验收证据与 QA 矩阵 | `docs/validation/`、`artifacts/evidence/` |
| 外部标准与文献 | `docs/standards/`、`docs/references/SCIENTIFIC_REFERENCES.md` |
| 术语 | `docs/GLOSSARY.md` |
| 开发与排查 | `docs/DEVELOPER_GUIDE.md`、`docs/TROUBLESHOOTING.md` |
