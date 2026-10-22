# ACSD v2 控制台（GUI）

独立 Qt6 Widgets 进程，只经**进程边界**与科学 CLI 交互：

* 启动 `acsd normalize|mosaic|export --json config.json` 子进程；
* 读 stdout 的 JSONL 事件流（`acsd.events/2`）；
* 需要时向子进程 stdin 写取消控制消息（`acsd.control/2`）；
* 读配置（`acsd.config/2`）与产品目录里的 `manifest.json`。

**边界（来自 `docs/engineering/rebuild/API.md` 与 `SYSTEM.md`）**

* 不解析任何内部 C++ 对象，不做任何科学运算，不依赖旧 ABI；
* 独立 CMake 工程（`project(acsd-gui)`），**不**加入根 `CMakeLists.txt`，
  不链接科学库，不 include `src/` 或 `include/acsd` 的内部头；
* 协议正本归 `docs/engineering/rebuild/API.md`，本目录只实现它。

---

## 1. 目录结构

```
gui/
  CMakeLists.txt          独立工程（acsd-gui / acsd-protocol-check）
  cmake/                  FindWrapOpenGL、FindXKB（最小环境下的 CMake 兜底，见 §6）
  src/
    protocol.{h,cpp}      事件协议：解析、seq/run_id 约束、取消报文、版本探测负载
    config.{h,cpp}        acsd.config/2 文档模型与本地校验
    runner.{h,cpp}        QProcess 运行器：运行/取消/强制终止、终事件判定
    product.{h,cpp}       产品目录 manifest.json 读取
    mainwindow.{h,cpp}    主窗口：配置编辑、输入分组、运行/取消、事件显示、产品浏览
    smoke.{h,cpp}         离屏冒烟模式（--smoke）
    main.cpp              入口
  tests/
    fake_acsd.py          假 CLI：只回放协议，不产生科学数据
    protocol_smoke.py     不依赖 Qt 的协议级冒烟（独立参考实现 + 与 GUI 代码交叉比对）
    protocol_check.cpp    无窗口协议检查器（复用 GUI 同一份协议/运行代码）
    run_offscreen_smoke.sh 离屏冒烟运行器（同时跑 GUI 冒烟与协议冒烟）
    sample-config.json    冒烟用配置样例
  evidence/               实际运行输出（冒烟日志与证据 JSON）
```

## 2. 构建

Linux（本机 Qt 6.7.3 装在 `~/qt6/6.7.3/gcc_64`）：

```bash
cmake -S gui -B build/gui -G Ninja -DCMAKE_PREFIX_PATH=$HOME/qt6/6.7.3/gcc_64
cmake --build build/gui
```

产物：

| 目标 | 说明 |
| --- | --- |
| `build/gui/acsd-gui` | 图形界面（含 `--smoke` 离屏冒烟模式） |
| `build/gui/acsd-protocol-check` | 无窗口协议检查器，复用 GUI 的协议/运行代码 |

Windows（Qt6/MSVC，与科学 CLI 的 CRT 无关，因为只走子进程）：

```bat
cmake -S gui -B build\gui -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\6.7.3\msvc2019_64
cmake --build build\gui --config Release
```

依赖：Qt 6（Core + Widgets）、CMake ≥ 3.25、支持 C++20 的编译器。
不依赖 Eigen/MPFR/CFITSIO/nlohmann_json 等科学库依赖。

## 3. 运行

```bash
QT_QPA_PLATFORM=offscreen build/gui/acsd-gui                 # 无 X11 环境下离屏
build/gui/acsd-gui --cli /path/to/acsd --config run.json      # 预置 CLI 与配置
build/gui/acsd-gui --smoke --config gui/tests/sample-config.json \
                   --cli gui/tests/fake_acsd.py \
                   --evidence gui/evidence/gui_smoke.json
```

`--smoke` 选项：`--scenario normal,cancel,crash,terminate`（默认四个全跑）、
`--cancel-delay-ms 400`。

## 4. 协议实现

### 4.1 配置（`acsd.config/2`）

GUI 编辑并写出 `acsd.config/2`：字段与 `schemas/config.schema.json` 一致
（`schema` / `command` / `inputs` / `scientific` / `output` / `catalogue` /
`resources` / `product_profile` / `publication`）。

* **版本探测**：读取配置时解析 `schema` 的 major；major ≠ 2 一律**拒绝加载并给出原因**
  （"配置 schema major N 与 GUI 支持的 2 不兼容，拒绝启动"）。
* 本地校验只做结构与 schema 一致性检查（错误会阻止运行，警告只提示）：
  科学的有效性由 CLI 判定，GUI 不做科学推理。
* 运行用的配置写在**用户配置文件同目录**下的 `acsd-gui-run-config.json`，
  以保证"配置路径相对 config 所在目录"的解析语义与用户配置一致。

### 4.2 事件（`acsd.events/2`）

stdout 每行 UTF-8 单对象。`EventParser`（`src/protocol.cpp`）逐行强制：

| 约束 | 违例处理 |
| --- | --- |
| `protocol` 必须是 `acsd.events/2` | 致命 → 本次运行判失败（不推断成功） |
| `run_id` 非空且一次 run 内唯一 | 致命 |
| `seq` 为整数 ≥ 0 且严格递增（首事件为 0） | 非递增致命；跳号记告警 |
| `event` 属于 7 个枚举值 | 未知事件名致命 |
| `code` / `message` 为字符串，`completed`/`total` 为整数 ≥ 0 | 致命 |
| `assessment` 为对象或 null | 致命 |
| `frame_id`（字符串/null）、`block`（整数/null）可选 | 类型错致命 |
| `run_finished` 必带 `exit_code`（0/2/3/4/5/6）与 `summary`（对象） | 缺字段致命；出现第二个 `run_finished` 致命 |
| 输出必须是合法 UTF-8 | 致命 |

**终事件缺失 → 判失败**：子进程结束但没有收到 `run_finished`（意外崩溃）时，
运行状态为"失败"，GUI 不会把它当作成功。

### 4.3 取消（`acsd.control/2`）

用户点"取消"时，GUI 向子进程 stdin 写**恰好一行**：

```
{"protocol":"acsd.control/2","command":"cancel","run_id":"<run_id>"}\n
```

* 键顺序与 API.md 的字面写法一致（不是 QJsonObject 的字典序）；
* `run_id` 取自首个事件；若用户先于首个事件取消，请求**排队**，收到 `run_started`
  后立即发出（界面会提示"取消请求已排队"）；
* 发出后 GUI **不自动 kill**，等待 CLI 在安全边界给出 `run_finished`（`exit_code=5` → 状态"已取消"）；
* 用户点"强制终止"时 GUI 杀掉子进程，本次运行标记为**已中止（aborted）**，
  绝不冒充完整结束。

### 4.4 退出码 → 界面状态

| `run_finished.exit_code` | 状态 |
| --- | --- |
| 0 | 成功（但 `run_summary` 里 `failed/unsolved/cancelled/...` 非零时降为"部分成功"） |
| 2 | 输入无效 |
| 3 | 能力不支持 |
| 4 | 失败 |
| 5 | 已取消 |
| 6 | 部分成功 |

### 4.5 版本探测

`acsd --version --json` 的输出被宽松解析（接受 `acsd.events/2` 形式或数值 `major`）。
事件协议 major ≠ 2 时 GUI **拒绝启动**并给出原因；探测不出 major 时不阻塞，
改由首个事件的 `protocol` 做强制校验。

## 5. 功能

* **配置编辑**：命令、输入表（路径 / `expected_hash` / `frame_id` / `calibration` / `response`，可增删排序）、
  科学文件、输出目录、星表、产品剖面、资源（`bytes`/`work`/`spill_bytes`/`threads`/`max_depth`/
  `max_precision_bits`/`scratch_directory`）、发布覆盖（`signal_dtype`/`tile_log2_width`/`durable`）。
  打开/保存 `acsd.config/2`，保存前做本地校验。
* **输入分组**：normalize 按 `frame_id` 分组，mosaic/export 按产品分组；
  选中分组可过滤事件表。
* **运行 / 取消**：选择 CLI、工作目录；运行、取消、强制终止；运行中禁用重复启动。
* **事件显示**：按 `seq` 顺序列出事件、帧、块、`code`、进度（`completed/total`）、消息；
  `issue` 高亮、协议错误行标红；聚合进度条；`run_summary` 与单事件 `assessment` 详情面板；
  完整 GUI 日志。
* **产品浏览**：选择产品目录 → 读 `manifest.json`，显示 stage、profile、complete、
  capabilities、objects/blocks 的产物路径、algorithm_set、原始 JSON。

## 6. 验证（实际运行输出见 `gui/evidence/`）

```bash
# 构建
cmake -S gui -B build/gui -G Ninja -DCMAKE_PREFIX_PATH=$HOME/qt6/6.7.3/gcc_64
cmake --build build/gui

# 离屏冒烟（GUI 冒烟 + 协议级冒烟）
gui/tests/run_offscreen_smoke.sh

# 或 ctest
ctest --test-dir build/gui --output-on-failure
```

* `gui/tests/protocol_smoke.py`：**不依赖 Qt**，用独立参考实现解析假 CLI 的 JSONL，
  验证 seq 递增、终事件唯一、取消报文字节完全一致、缺终事件必须判失败；
  若同时给出 `--check <acsd-protocol-check>`，还会用 **GUI 自己的 C++ 协议代码**
  跑同样场景，并把两套实现的规范化事件流逐行对比。
* `acsd-gui --smoke`：真实创建窗口（`QT_QPA_PLATFORM=offscreen`）、加载配置、
  连接 CLI、收 `run_finished`、发一次取消、读产品 manifest、验证不兼容配置被拒绝。

### 无 OpenGL 开发包的最小环境

Qt6Gui 在链接期需要 `libGL.so.1` / `libEGL.so.1`（它自己的 `DT_NEEDED`），
但不需要 GL 头文件。`gui/cmake/FindWrapOpenGL.cmake` 与 `FindXKB.cmake` 会在
缺少 `libgl-dev` / `libxkbcommon-dev` 时提供兜底目标，并在
`ACSD_GUI_OPENGL_LIB_DIR`（或 `~/.cache/wklibs/usr/lib/x86_64-linux-gnu`、
系统库目录）里找到运行期库后写进构建期 rpath。
`run_offscreen_smoke.sh` 也会在 `ldconfig` 找不到这两个 so 时补 `LD_LIBRARY_PATH`。

## 7. 限制与未完成

* **真实 CLI 未就绪**：科学 CLI 由 `apps` Agent 实现（`apps/cli/` 目前只有 README）。
  本目录的验证用**假 CLI**（`gui/tests/fake_acsd.py`）回放符合
  `schemas/events.schema.json` 的 JSONL，只验证 GUI 侧的协议行为，
  **不构成对真实 CLI 的协议验收**；真实 CLI 可用后需补一次真协议冒烟。
* GUI 不做科学解释：`run_summary` 的字段名未在正本固定，因此"部分成功"的判定
  采用宽松匹配（`failed`/`unsolved`/`cancelled`/`unpublished`/`incomplete`/`partial`/
  `errors` 等键的非零计数，或 `complete=false`）。真实 CLI 定稿后应收紧为固定字段。
* `manifest.json` 的读取按 `ProductManifest` 的字段名实现，并对 `stage`
  容忍字符串（`normalize`/`mosaic`/`export_image`）与序号两种写法；
  其余枚举字段未做额外猜测。
* 产品浏览只读目录，不解析产品携带的科学对象、不执行产品内的任何内容。
* 输入分组是**派生视图**（`acsd.config/2` 没有分组字段），不写回配置。
* Windows 下用 Qt6/MSVC 构建；本轮只在 Linux amd64 + Qt 6.7.3 上实跑过。
