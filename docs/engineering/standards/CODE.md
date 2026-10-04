# 代码标准

上游：最高设计的模块与 ABI、双平台发行两章；架构边界见 `../architecture/ARCHITECTURE.md`[2]。

代码必须遵守的条目、命名与机器契约保留面与编译器告警口径。注释纪律见 `COMMENT.md`。

权威来源：`../../ACSD_DESIGN.md` 第 11 节（双平台发行，C++17 双平台工具链：**Windows = MSVC v143**；
Linux = GCC 或 Clang；Windows 官方工具链为 MSVC）与第 8.5 节（模块与 ABI，每个可独立调度模块的必备项）。
本文件是这些要求的实现级展开[1]。

## MUST

- C++17（**正式 toolchain：Windows = MSVC v143；Linux = GCC 或 Clang**，见 `../../ACSD_DESIGN.md` 第 11 节
 「双平台发行」与第 8.5 节「模块与 ABI」）；RAII 优先。
 MinGW64 的定位 = **本地开发/兼容性验证**工具链；正式工具链取上条所列工具链、
 发布物构建依据同样取正式工具链。工具链版本取值由 `DEPENDENCY.md` 登记的机器单一事实源给出，
 本文件不复述。
- 公共指针必须声明所有权（borrowed / owned / optional）与空值语义[1]。
- 分配前检查尺寸运算；整数乘法一律经检查后再分配。
- 异常边界 = C++ 侧；C ABI 失败时输出重置，单出口 cleanup/RAII。
- 稳定 success / recoverable science status / hard error 三层语义；
 三层语义各占其一（`rc=0 + invalid status` 即双语义违规）。
- 科学状态只经显式 model handle；hidden global mutable science state 不作承载面。
- config fallback 一律显式并保持科学语义；默认值两处不一致视为缺陷。
- production science implementation 单一（单一实现 + oracle）。
- 热路径按无 per-pixel malloc/new、无 per-pixel I/O、无 per-pixel log/clock 的口径编写；
 run constant 只计算一次；cache 必须有 capacity/identity/invalidation/thread model。
- 每个 fast path 必须有 reference path + equivalence oracle + failure fallback。
- 每个数值科学量文档化单位/坐标系/归一化/精度需求/有限域[3]。
- 外部进程/网络/硬件等待必须显式 timeout。
- science product 写盘：temp write → validate → atomic promote。

## SHOULD

- immutable run context；thread-local 可复用 scratch。
- typed config；enum/status 替代 magic number；窄接口。
- 确定性测试。

## MAY

- compatibility reader（仅当与 production solver/writer 隔离）。

## 编译器告警口径

告警口径按「同一份源码在两个平台上语义一致」的原则定，不按单平台零告警定。

### MSVC 咨询性告警的项目级豁免

- `C4996` 是 MSVC 的**咨询性**诊断：触发它的是 ISO C / POSIX 名（`strncpy`、`fopen`、`getenv`、`_open`、`strerror`、`strdup` 等），同一份代码在 GCC / Clang 侧零告警。它是平台口径差，不是代码缺陷；
- 处置方式是在 MSVC 配置面（根 `CMakeLists.txt` 的 MSVC 分支）**项目级**定义 `_CRT_SECURE_NO_WARNINGS` 与 `_CRT_NONSTDC_NO_WARNINGS`，不逐个调用点重写——数百处重写的回归风险高于收益；
- 该豁免**只覆盖这两个咨询宏**。其余告警类别（C4244、C4334、C4324、C4310、C4190、C4100、C4127、C4456 等）一律逐点真修，或按语言标准正当处理；
- 禁止 `/w`、`/W0` 一类整目标或全局降级来掩盖本项目源码的告警。

### 第三方头的窄隔离

- vendored 第三方头（如 `nanoflann.hpp`）按其自身触发的诊断类别，在**引入点**做窄隔离：`#pragma warning(push/pop)` 或以 `SYSTEM` 方式包含；
- 隔离处必须写注释说明三件事：这是第三方头、依据是什么、影响面多大；
- 窄隔离不构成降级本项目源码告警口径的依据。

## 命名：显示名与机器契约保留面

**显示名（唯一）**：`ACSD`，全称 `Astro Celestial Sphere Database`。正文、文档、界面、注释、报告与提交消息一律使用显示名。机器契约保留面按下列判定规则处理。

**判定规则**：把该处的 `AstroCS` 族字面量换成显示名 `ACSD`，**看是否有任何机器会因此失配或指向不存在的对象**——编译器与链接器（include 路径、符号名）、`git`（忽略模式）、CMake（target 与变量）、schema 与合同校验（键名、ID、字面量）、CI 匹配（workflow 名、artifact 名、路径）、测试断言、台账与文档锚。**会失配 ⇒ 它是机器契约，按字面保留；不会失配 ⇒ 它是显示名，必须写成 `ACSD`。没有第三种状态。**

**机器契约保留面（按类枚举；这些字面量被机器按字面读取）**：

| # | 类 | 保留面 | 按字面保留的依据 |
|---|---|---|---|
| 1 | C/C++ include 与符号 | 公共头目录 `acsd/`、`#include <acsd/…>`、`namespace acsd`、`acsd::`、`acsd_*.dll` / `.so` / `.a` | 编译面与链接面按字面解析 |
| 2 | 合同 / 注册表 / 模块 ID | schema 注解键 `x-acsd*`；点分、连字符与斜杠 ID：`acsd.*`、`MOD-acsd-*`、`acsd-*`、`acsd/<x>/vN`；台账 schema id 的 `acsd*` | schema 锚、注册表与产品清单按字面匹配 |
| 3 | 环境变量 / CMake 选项 / 根文档名 | `ACSD_*`、`../../ACSD_DESIGN.md`，以及**文档与台账内**的轮次标识 `ACSD-*` | 构建入口按字面读取；根文档名是全仓行锚的宿主 |
| 4 | 可执行 / target / CLI 名 | `acsd`、`acsd.exe`、`acsd-cli`、CI artifact 前缀 `acsd-windows-candidate-` | 构建 target、CI 选择器与单测断言 |
| 5 | CI workflow 名 | `ACSD Linux CI` / `ACSD Windows CI` / `ACSD Fatduck Validation` | `workflow_run.workflows` 与 CI 选择器按名精确匹配 |
| 6 | CI 候选产物成员名 | `ACSD-candidate.zip` 及其落盘路径 | 常量、`require_outputs`、工作流绑定与单测 |
| 7 | 冻结的宿主安装路径 | `C:/AstroCS/toolchains/…`、`D:\AstroCSRunner\…` | preset 与依赖锁冻结的安装位；SBOM 白名单正则与工作流按字面断言 |
| 8 | 注册目录名与发布 / 审核产物名 | 根目录名 `ACSD.wiki/`；`dist/ACSD-CLI-v1/…`、`ACSD-<根 VERSION>-win-x64/`、`ACSD-audit-*` | `git` 忽略模式、根清单登记、安装树合同与生成器常量 |
| 9 | 对外协议标识 | HTTP `User-Agent` product token，形如 `<产品标识>/<主>.<次>`，例 `ACSD-<产品标识>/1.0` | 对外声明的客户端身份；改名是对外行为改变，不属命名统一的范围 |
| 10 | 封存证据与只读数据登记面 | `artifacts/evidence/**`（证据锚按封存时的文件名与标题逐字引用）、`testdata/**`（只读登记目录，其 `README.md` 规定目录内含数据按只读处理） | 证据锚指向的对象一经封存即保持原样；只读目录的处置属发布权范畴[1] |

**轮次标识的管辖边界（两类面互斥）**：

| 面 | 管辖文件集 | 处置 |
|---|---|---|
| 文档与台账内的轮次标识 | `docs/**`、`run/**`、`实验/**` 的正文与台账 | 按字面保留；它是实验单元与审核包的可寻址标识[2] |
| 生产源码注释内的轮次标识 | `lib/**`、`eng/**` 的 `.c`/`.cpp`/`.h`/`.hpp` 注释 | 按 `COMMENT.md` 的必须删除/迁移一节删除；代码注释不承载轮次叙事 |

两面的判据是**文件集**而非字面形状：同一个 `ACSD-*` 字面出现在文档里是标识，出现在源码注释里是叙事。

**唯一源与机器门**：

- 本节是显示名与机器契约保留面的**唯一定义源**；其它文档、检查器与台账只引用本节，不复述本节定义；
- 类级登记与逐类判据只登记类、判据与机器依据，不重复本节定义；
- 逐类机器判据由命名面检查项执行，判据是「本节所列保留类的字面集合」与「全仓实际出现的字面集合」
 的差集为空。**本标准不声明该检查项的执行面**：检查项未在位时，本节各行一律按**人工判读**执行，
 不得在任何台账或判词中记为「门已生效」。

## 关联

- NUMERIC.md、`../api/PUBLIC_API.md`、CONCURRENCY.md、`../contracts/LOG_AND_ERROR.md`、
 `../contracts/ATOMIC_PUBLISH.md`；工具链版本见 `DEPENDENCY.md`[5]。

- C++17；MSYS2 MinGW64 g++（**本地开发/兼容性验证工具链**，版本取值由 `DEPENDENCY.md` 登记的
 机器单一事实源给出）；OpenMP 仅显式并行区。
- `.clang-format`（根目录）覆盖 first-party；third_party 不
 mass-format；`.editorconfig`（根目录）统一缩进/换行/编码[2]。
- 命名：类型 `PascalCase`（C 结构 `P2`/`Aio` 前缀保留 ABI）、函数
 `snake_case`、成员 `snake_case_`（C++ 类）。
- include 顺序：本模块 → acsd 头 → std；self-contained public header。
- C ABI：`extern "C"`、不抛异常、buffer ownership/lifetime/nullable/单位
 注释齐全；return/status 集中定义；logging 与 status 分离。
- 警告策略：`-Wall -Wextra` first-party 无新增警告。

## 注释

注释纪律（原则、必须注释、必须删除/迁移、叙述性注释的清理面、长度、审计）见 `COMMENT.md`；
本文件只承载代码条款，单向引用 `COMMENT.md`，不复制其条目[4]。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md`，最高设计，第 11 节（双平台发行）与第 13 节（版本与发布权），上位来源；
 本篇的 MUST 与 SHOULD 条目是第 8.5 节（模块与 ABI）要求的实现级展开。
[2] 内部文档 `docs/engineering/architecture/ARCHITECTURE.md`，架构边界，同层相关正本。
[3] 内部文档 `docs/engineering/standards/NUMERIC.md`，数值标准，同层相关正本。
[4] 内部文档 `docs/engineering/standards/COMMENT.md`，注释纪律，本篇注释条款的展开。
[5] 内部文档 `docs/engineering/standards/DEPENDENCY.md`，依赖规则，工具链与依赖版本取值的单一事实源登记面。
