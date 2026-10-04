# 代码标准

上游：最高设计的模块与 ABI、双平台发行两章；架构边界见 `../architecture/ARCHITECTURE.md`。

代码必须遵守的条目、命名与机器契约保留面、注释纪律与编译器告警口径。

权威来源：`../../ACSD_DESIGN.md` 「双平台发行」一节（双平台发行）（C++17 双平台工具链：**Windows = MSVC v143**；Linux = GCC 或 Clang）
与 （每模块必备项）+ `../../ACSD_DESIGN.md` （官方 Windows 工具链 = MSVC）。本文件是这些要求的实现级展开。

## MUST

- C++17（**正式 toolchain：Windows = MSVC v143；Linux = GCC 或 Clang**，见 `../../ACSD_DESIGN.md` 「双平台发行」一节（双平台发行）
 与 `../../ACSD_DESIGN.md` 「官方 Windows 工具链 = MSVC」）；RAII 优先。
 MinGW64 的定位 = **本地开发/兼容性验证**工具链；正式工具链取上条所列工具链、
 发布物构建依据同样取正式工具链。
- 公共指针必须声明所有权（borrowed / owned / optional）与空值语义。
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
- 每个数值科学量文档化单位/坐标系/归一化/精度需求/有限域。
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
| 3 | 环境变量 / CMake 选项 / 根文档名 | `ACSD_*`、`../../ACSD_DESIGN.md`，以及轮次标识面 `ACSD-*` | 构建入口按字面读取；根文档名是全仓行锚的宿主 |
| 4 | 可执行 / target / CLI 名 | `acsd`、`acsd.exe`、`acsd-cli`、CI artifact 前缀 `acsd-windows-candidate-` | 构建 target、CI 选择器与单测断言 |
| 5 | CI workflow 名 | `ACSD Linux CI` / `ACSD Windows CI` / `ACSD Fatduck Validation` | `workflow_run.workflows` 与 CI 选择器按名精确匹配 |
| 6 | CI 候选产物成员名 | `ACSD-candidate.zip` 及其落盘路径 | 常量、`require_outputs`、工作流绑定与单测 |
| 7 | 冻结的宿主安装路径 | `C:/AstroCS/toolchains/…`、`D:\AstroCSRunner\…` | preset 与依赖锁冻结的安装位；SBOM 白名单正则与工作流按字面断言 |
| 8 | 注册目录名与发布 / 审核产物名 | 根目录名 `ACSD.wiki/`；`dist/ACSD-CLI-v1/…`、`ACSD-<根 VERSION>-win-x64/`、`ACSD-audit-*` | `git` 忽略模式、根清单登记、安装树合同与生成器常量 |
| 9 | 对外协议标识 | HTTP `User-Agent` product token（形如 `ACSD-BASS-Index/1.0`） | 对外声明的客户端身份；改名是对外行为改变，不属命名统一的范围 |
| 10 | 封存证据与只读数据登记面 | `artifacts/evidence/**`（证据锚按封存时的文件名与标题逐字引用）、`testdata/**`（只读登记目录，其 `README.md` 规定目录内含数据按只读处理） | 证据锚指向的对象一经封存即保持原样；只读目录的处置属发布权范畴（最高设计 ） |

**唯一源与机器门**：

- 本节是显示名与机器契约保留面的**唯一定义源**；其它文档、检查器与台账只引用本节，不复述本节定义；
- 类级登记与逐类判据（只登记类、判据与机器依据，不重复本节定义）；
- 门 = `CHK-NAMING-SURFACE`：以 `git grep -w` 扫三族字面量（混写 `ACSD`、小写别名 `acsd`、全大写命名空间 `ACSD`），**每一处命中必须落在某一保留类内**；落在类外判红，即显示名漏改。门自带 `--self-test` 负例面与定义面棘轮（只减不增）。

## 关联

- NUMERIC.md、`../api/PUBLIC_API.md`、CONCURRENCY.md、`../contracts/LOG_AND_ERROR.md`、
 `../contracts/ATOMIC_PUBLISH.md`。

- C++17；MSYS2 MinGW64 g++ 16.1.0；OpenMP 仅显式并行区。
- `.clang-format`（根目录，V14 落地）覆盖 first-party；third_party 不
 mass-format；`.editorconfig`（根目录）统一缩进/换行/编码。
- 命名：类型 `PascalCase`（C 结构 `P2`/`Aio` 前缀保留 ABI）、函数
 `snake_case`、成员 `snake_case_`（C++ 类）。
- include 顺序：本模块 → acsd 头 → std；self-contained public header。
- C ABI：`extern "C"`、不抛异常、buffer ownership/lifetime/nullable/单位
 注释齐全；return/status 集中定义；logging 与 status 分离。
- 警告策略：`-Wall -Wextra` first-party 无新增警告。

## 原则

Comment 解释 WHY / SCIENCE / INVARIANT / OWNERSHIP / THREAD-SAFETY /
NON-OBVIOUS PERFORMANCE；Code 解释 WHAT；历史沿革进 ADR 与 git 提交消息。

## 必须注释

1. 科学公式：变量/单位 + SCI/ALG ID（完整推导放 science/algorithm docs）。
2. 非显然 invariant。
3. conservative geometry：明示 false positive allowed / false negative forbidden。
4. ownership/lifetime。
5. thread-safety（shared/thread-local/reduction）。
6. unusual numeric constant 来源。
7. 非显然性能决策。
8. workaround 必须链接 issue/ADR。

## 必须删除/迁移

production code 注释的清理面 = V[0-9]+、R[0-9]+、MICROFIX、控制包、审计轮次、
骨架版本、第??号计划、"本次修复"、"历史原因如下"、"以后 Task 再做"。
允许 whitelist：API protocol version、FITS/HiPS formal version、
scientific model version。

## 叙述性注释的清理面

删除 "// 初始化变量"、"// 遍历数组"、"// 写文件"、"// 返回成功" 等叙述性注释。

## 长度

- 普通非显然逻辑：1–4 行；数学 derivation 源码只留结论 + ID；
- 超过 8–12 行历史/推导注释优先迁文档。

## 科学代码推荐写法

```cpp
// SCI-DRZ-004 / ALG-DRZ-OVERLAP-002:
// Conservative reject; false positives are allowed, false negatives are not.
```

## 审计

每个 production 文件记录 comment_hygiene = PASS/FAIL，由注释卫生检查项产出并随该检查项的运行产物落盘。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md，最高设计`，上位来源。
[2] 内部文档 `docs/engineering/architecture/ARCHITECTURE.md`，同层相关正本。
[3] 内部文档 `docs/engineering/standards/NUMERIC.md`，同层相关正本。
