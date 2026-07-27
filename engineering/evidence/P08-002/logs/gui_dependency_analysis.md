# GUI 依赖分析报告 (P08-002)

## 分析目标

验证 healpix_browser_qt (GUI) 可只依赖 CLI 契约（orchestrator.exe inspect）或格式契约
（hiss_format_v1.md / hcsd_format_v1.md），而不直接依赖 orchestrator.exe 的内部 C++ 逻辑。

## 分析对象

- 模块: `lib/healpix_db/healpix_browser_qt/`
- 三层架构: `core/` (纯 C++17 + OpenGL, 无 Qt) + `widgets/` (Qt6 封装) + `app/` (demo)
- 关键文件: `CMakeLists.txt`, `core/browser_backend.h`, `core/browser_backend.cpp`

## 链接依赖分析

### CMakeLists.txt 声明的链接库

| 目标 | 链接库 |
|---|---|
| healpix_browser_core (静态库) | OpenGL::GL, gdi32, **astro_image_io** |
| healpix_browser_qt_widgets (静态库) | healpix_browser_core, Qt6::Core, Qt6::Gui, Qt6::OpenGLWidgets |
| healpix_browser_qt (demo exe) | healpix_browser_qt_widgets, healpix_browser_core, Qt6::Core, Qt6::Gui, Qt6::Widgets |

### 关键发现

1. **链接 astro_image_io.dll** (独立 I/O 库, 非 orchestrator 内部):
   - `target_link_directories(healpix_browser_core PUBLIC ${AIO_DIR})`
   - `target_link_libraries(healpix_browser_core PUBLIC astro_image_io)`
   - 提供: `aio_hiss_read`, `aio_hcsd_read`, `aio_hcsd_read_leaf`, `aio_hio_free` (兼容宏 `hiss_read` 等)
   - astro_image_io.dll 是独立模块, 在 v1.1 发布包中作为单独 DLL 分发 (`lib/astro_image_io/astro_image_io.dll`)

2. **未链接 orchestrator.exe 或其内部库**:
   - 链接列表中无 orchestrator 相关条目
   - 未引用 orchestrator 内部头文件
   - GUI 通过直接读 HISS/HCSD 文件格式获取数据, 不调用 orchestrator 进程

3. **Qt6 + OpenGL**:
   - widgets/ 依赖 Qt6 (Core/Gui/Widgets/OpenGLWidgets)
   - core/ 依赖 OpenGL::GL + gdi32 (Windows GDI 用于 OpenGL 上下文)
   - 这些是 GUI 框架依赖, 与 orchestrator 业务逻辑无关

## 数据读取路径分析

### browser_backend.h 声明的 I/O 接口

```cpp
class BrowserBackend {
public:
    int open_file(const std::string& path);  // 打开 .hiss 或 .hcsd
    bool is_hiss() const;                    // 单帧模式
    bool is_hcsd() const;                    // 球面模式
    uint32_t get_nside() const;
    uint64_t get_n_pix() const;
    std::vector<uint64_t> get_required_leaves(const ViewParams& view) const;
    LeafData load_leaf(uint64_t leaf_ipix, uint32_t target_nside);
    // ...
};
```

### 实际调用链

GUI 打开 HISS/HCSD 文件 → `BrowserBackend::open_file(path)` →
内部调用 `astro_image_io.dll` 的 `aio_hiss_read` / `aio_hcsd_read` →
按 HISS/HCSD 公开格式规范读取文件二进制数据。

### 契约合规性

| 契约路径 | 是否使用 | 说明 |
|---|---|---|
| 格式契约 (HISS/HCSD 公开格式) | **是** | 直接读文件格式, 通过 astro_image_io.dll |
| CLI 契约 (orchestrator.exe inspect) | 否 | GUI 不调用 orchestrator.exe |
| 内部逻辑 (orchestrator C++ 源码) | 否 | GUI 不链接 orchestrator 内部库 |

**结论**: GUI 走**格式契约路径** (合法路径), 通过独立 I/O 库 astro_image_io.dll 直接读 HISS/HCSD 文件,
不依赖 orchestrator.exe 内部逻辑。

## CLI 契约路径验证 (smoke 测试)

虽然 GUI 不使用 CLI 契约路径, 但 P08-002 smoke 测试已验证该路径可用:

- `orchestrator.exe inspect --hiss <file>` → JSONL result 事件 (含 nside/n_pix/filter/exposure_s)
- `orchestrator.exe inspect --hcsd <file>` → JSONL result 事件 (含 nside/n_pix/n_leaves/n_frames)
- `orchestrator.exe inspect --hiss nonexistent.hiss` → exit 8 (FILE_IO_ERROR) + JSONL error 事件

未来 v1.2+ 可实现 `BrowserBackendCli` 类, 通过子进程调用 orchestrator.exe inspect + 解析 JSONL,
实现完全解耦的 CLI 契约路径 (作为格式契约路径的替代方案)。

## 当前状态

- **源码完整**: lib/healpix_db/healpix_browser_qt/ 已实现 core/widgets/app 三层
- **编译验证**: CMake 34/34 编译成功 (MinGW-w64 g++ 16.1.0 + Qt6)
- **单元测试**: 13/13 PASS (test_healpix_math 5/5 + test_stf_engine 4/4 + test_browser_backend 4/4)
- **未包含在 v1.1 发布包**: v1.1 仅发布 CLI Core, GUI 留待 v1.2+ 发布
- **不阻塞 v1.1 交付**: GUI 状态不影响 CLI 发布包功能

## 裁决

**VERDICT: PASS**

GUI (healpix_browser_qt) 满足只依赖 CLI 契约/格式契约的要求:
1. 通过 astro_image_io.dll (独立 I/O 库) 直接读 HISS/HCSD 公开格式 (格式契约路径)
2. 不链接 orchestrator.exe 内部库, 不调用 orchestrator 进程
3. smoke 测试已证明 CLI 契约路径 (orchestrator inspect) 可用, 未来可作为替代方案
4. GUI 当前未包含在 v1.1 发布包, 不阻塞 v1.1 交付
