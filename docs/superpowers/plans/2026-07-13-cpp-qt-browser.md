# C++ Qt HEALPix 浏览器实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用 C++ + Qt6 重写 HEALPix 浏览器，消除现有 WebGL 架构的 HTTP + base64 通讯开销，核心算法库无 Qt 依赖可嵌入大工程。

**Architecture:** 三层架构：core/（纯 C++17 + OpenGL，无 Qt 依赖）→ widgets/（Qt6 QOpenGLWidget 封装）→ app/（demo exe）。数据通过 C++ 内存直传 OpenGL 纹理，零序列化开销。UI 与算法严格分离。

**Tech Stack:** C++17, OpenGL 3.3 Core, Qt6 (Widgets/OpenGLWidgets), MSYS2 MinGW64 g++ 16.1.0, healpix_io.dll（现有），Makefile（core）+ CMake（Qt 层）

**Spec 文档：**
- 核心算法：`docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md`
- UI 前端：`docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md`

---

## 文件结构总览

```
lib/healpix_db/healpix_browser_qt/
├── core/                          ← 算法核心（无 Qt 依赖）
│   ├── healpix_math.h/.cpp        ← 球面坐标转换
│   ├── stf_engine.h/.cpp          ← STF 显示拉伸
│   ├── browser_backend.h/.cpp     ← 数据加载/按需子叶
│   ├── gl_renderer.h/.cpp         ← OpenGL 渲染
│   └── logger.h                   ← 日志宏
├── widgets/                       ← Qt6 widget 层
│   ├── abstract_view.h/.cpp
│   ├── single_frame_view.h/.cpp
│   └── sphere_view.h/.cpp
├── app/                           ← demo exe
│   ├── main.cpp
│   ├── main_window.h/.cpp
│   └── stf_panel.h/.cpp
├── include/
│   └── healpix_browser_core.h     ← 对外统一头文件
├── tests/                         ← 单元测试（assert + main）
│   ├── test_healpix_math.cpp
│   ├── test_stf_engine.cpp
│   ├── test_browser_backend.cpp
│   └── run_tests.bat
├── Makefile                       ← core/ 静态库 + 测试
├── CMakeLists.txt                 ← widgets/ + app/（Qt6）
├── memory.md                      ← 模块开发 memory
└── README.md
```

**归档操作（最后阶段）：**
- `lib/healpix_db/healpix_browser_cpp/` → `lib/healpix_db/archive/healpix_browser_cpp/`
- `lib/healpix_db/healpix_browser_web/` → `lib/healpix_db/archive/healpix_browser_web/`

---

## Task 1: 模块初始化与目录结构

**Files:**
- Create: `lib/healpix_db/healpix_browser_qt/Makefile`
- Create: `lib/healpix_db/healpix_browser_qt/memory.md`
- Create: `lib/healpix_db/healpix_browser_qt/core/logger.h`
- Create: `lib/healpix_db/healpix_browser_qt/README.md`

- [ ] **Step 1: 创建目录结构**

Run（PowerShell，UTF-8 环境）：
```powershell
# 全局强制UTF-8编码
[Console]::InputEncoding = [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
[System.Text.Encoding]::Default = [System.Text.Encoding]::UTF8

$base = "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt"
New-Item -ItemType Directory -Force -Path "$base\core","$base\widgets","$base\app","$base\include","$base\tests"
```
Expected: 5 个目录创建成功

- [ ] **Step 2: 写 logger.h（日志宏，与现有模块风格一致）**

写入 `lib/healpix_db/healpix_browser_qt/core/logger.h`：
```cpp
#ifndef BROWSER_LOGGER_H
#define BROWSER_LOGGER_H

#include <cstdio>
#include <cstdarg>
#include <ctime>

namespace browser_log {

inline void log(const char* level, const char* fmt, ...) {
    time_t now = time(nullptr);
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stderr, "[%s][%s] ", tbuf, level);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    fflush(stderr);
}

}  // namespace browser_log

#define LOG_INFO(fmt, ...)  browser_log::log("INFO",  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  browser_log::log("WARN",  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) browser_log::log("ERROR", fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) browser_log::log("DEBUG", fmt, ##__VA_ARGS__)

#endif
```

- [ ] **Step 3: 写 Makefile（core 静态库 + 测试，先放骨架）**

写入 `lib/healpix_db/healpix_browser_qt/Makefile`：
```makefile
# Makefile - HEALPix 浏览器 Qt 版 (healpix_browser_qt)
# 用途: 编译 libhealpix_browser_core.a 静态库 + 单元测试
# 依赖: MSYS2/MinGW64 (g++), healpix_io.dll (在 ../healpix_io/)

CXX = g++
CXXFLAGS = -O2 -std=c++17 -Wall -Wextra -fopenmp
INCDIR = core include
HIO_DIR = ../healpix_io

# core 静态库源文件
CORE_SRCS = core/healpix_math.cpp core/stf_engine.cpp \
            core/browser_backend.cpp core/gl_renderer.cpp
CORE_OBJS = $(CORE_SRCS:.cpp=.o)

# 测试
TESTS = tests/test_healpix_math.exe tests/test_stf_engine.exe tests/test_browser_backend.exe

.PHONY: all clean tests core

all: core tests

# 编译 core 静态库
core: libhealpix_browser_core.a

libhealpix_browser_core.a: $(CORE_OBJS)
	ar rcs $@ $^

# 编译规则
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(addprefix -I,$(INCDIR)) -I$(HIO_DIR)/include -c $< -o $@

# 测试目标（单独编译每个测试）
tests/test_healpix_math.exe: tests/test_healpix_math.cpp core/healpix_math.cpp
	$(CXX) $(CXXFLAGS) $(addprefix -I,$(INCDIR)) -I$(HIO_DIR)/include -o $@ $^ -L$(HIO_DIR) -lhealpix_io

tests/test_stf_engine.exe: tests/test_stf_engine.cpp core/stf_engine.cpp
	$(CXX) $(CXXFLAGS) $(addprefix -I,$(INCDIR)) -o $@ $^

tests/test_browser_backend.exe: tests/test_browser_backend.cpp core/browser_backend.cpp core/healpix_math.cpp
	$(CXX) $(CXXFLAGS) $(addprefix -I,$(INCDIR)) -I$(HIO_DIR)/include -o $@ $^ -L$(HIO_DIR) -lhealpix_io

tests: $(TESTS)

# 运行所有测试
run_tests: tests
	@for t in $(TESTS); do echo "=== $$t ==="; ./$$t || exit 1; done

clean:
	del /Q *.o core\*.o 2>nul
	del /Q libhealpix_browser_core.a 2>nul
	del /Q tests\*.exe 2>nul
```

- [ ] **Step 4: 写 memory.md（模块开发 memory 初始版）**

写入 `lib/healpix_db/healpix_browser_qt/memory.md`：
```markdown
# healpix_browser_qt 模块开发 Memory

## 模块定位
- 替代 healpix_browser_cpp（C++ HTTP 后端）+ healpix_browser_web（WebGL 前端）
- 三层架构：core/（无 Qt）→ widgets/（Qt6）→ app/（demo exe）
- 消除 HTTP + base64 通讯开销，C++ 内存直传 OpenGL 纹理

## 设计文档
- 核心算法：docs/superpowers/specs/2026-07-13-cpp-qt-browser-core-design.md
- UI 前端：docs/superpowers/specs/2026-07-13-cpp-qt-browser-ui-design.md

## 当前进度
- [x] Task 1: 模块初始化与目录结构
- [ ] Task 2: HealpixMath
- [ ] Task 3: STFEngine
- [ ] Task 4: BrowserBackend
- [ ] Task 5: GLRenderer
- [ ] Task 6-8: widgets/
- [ ] Task 9-11: app/
- [ ] Task 12: CMake 构建
- [ ] Task 13: 归档与文档

## 关键约束
- core/ 无 Qt 依赖（grep -r "Q" core/ 应无 Qt 类型引用）
- OpenGL 3.3 Core Profile
- nside=8192 精度用 uint64_t
- STF MTF 公式：((m-1)*x) / ((2m-1)*x - m)
- 球面渲染用方案 B（顶点查值，CPU 端每顶点查值作为 attribute）

## 依赖
- healpix_io.dll（hiss_read/hcsd_read/hcsd_read_leaf）
- Qt6（widgets/app 层）
- OpenGL32, gdi32（系统）
```

- [ ] **Step 5: 写 README.md**

写入 `lib/healpix_db/healpix_browser_qt/README.md`：
```markdown
# healpix_browser_qt

C++ + Qt6 实现的 HEALPix 浏览器，替代 WebGL 版本。

## 架构
- `core/` - 核心算法库（纯 C++17 + OpenGL，无 Qt 依赖）
- `widgets/` - Qt6 QOpenGLWidget 封装层
- `app/` - demo 可执行程序

## 编译
### core 静态库（Makefile）
```bash
make core
```

### 完整程序（CMake，需 Qt6）
```bash
mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH="C:/msys64/mingw64" ..
cmake --build .
```

## 测试
```bash
make run_tests
```
```

- [ ] **Step 6: 验证目录结构**

Run：
```powershell
$base = "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt"
Get-ChildItem -Path $base -Recurse -File | Select-Object FullName
```
Expected: 看到 logger.h, Makefile, memory.md, README.md 4 个文件

- [ ] **Step 7: Commit（项目根非 git 仓库，记录到 memory.md 即可）**

由于项目根目录不是 git 仓库（各模块独立仓库），本模块暂不初始化 git。Task 13 阶段决定是否独立建仓库。在 memory.md 中记录 Task 1 完成。

---

## Task 2: HealpixMath（球面坐标转换）

**Files:**
- Create: `lib/healpix_db/healpix_browser_qt/core/healpix_math.h`
- Create: `lib/healpix_db/healpix_browser_qt/core/healpix_math.cpp`
- Test: `lib/healpix_db/healpix_browser_qt/tests/test_healpix_math.cpp`

**移植来源**：`healpix_browser_web/js/webgl-renderer.js` 的 `pix2angNest` 方法（C++ 回迁）。

- [ ] **Step 1: 写头文件**

写入 `core/healpix_math.h`（内容见 spec §3.3.1，完整接口定义）。

- [ ] **Step 2: 写失败的测试**

写入 `tests/test_healpix_math.cpp`：
```cpp
#include <cassert>
#include <cmath>
#include <cstdio>
#include "healpix_math.h"

static bool approx(double a, double b, double eps = 1e-6) {
    return std::fabs(a - b) < eps;
}

void test_pix2ang_nest_nside1() {
    // nside=1，12 个基础像素
    // ipix=0 → 北极区，ra≈0, dec≈90 (HEALPix 标准)
    double ra, dec;
    HealpixMath::pix2ang_nest(1, 0, ra, dec);
    // 北极像素 dec 应在 [26.57, 90] 范围
    assert(dec > 26.0 && dec <= 90.0);

    // ipix=3 → 赤道区
    HealpixMath::pix2ang_nest(1, 3, ra, dec);
    assert(approx(std::fabs(dec), 0.0, 30.0));  // 赤道附近
    printf("[PASS] test_pix2ang_nest_nside1\n");
}

void test_ang2pix_roundtrip() {
    // 往返一致性：pix2ang → ang2pix 应返回相同 ipix
    for (uint32_t nside : {1u, 2u, 8u, 64u}) {
        for (uint64_t ipix = 0; ipix < 12ULL * nside * nside; ipix += 12ULL * nside * nside / 12) {
            double ra, dec;
            HealpixMath::pix2ang_nest(nside, ipix, ra, dec);
            uint64_t ipix2 = HealpixMath::ang2pix_nest(nside, ra, dec);
            assert(ipix == ipix2);
        }
    }
    printf("[PASS] test_ang2pix_roundtrip\n");
}

void test_query_disc() {
    // 查询 RA=0, Dec=0, radius=10° 的圆盘
    auto result = HealpixMath::query_disc(64, 0.0, 0.0, 10.0);
    assert(!result.empty());
    // 所有返回 ipix 距中心应 < 10°
    for (uint64_t ipix : result) {
        double ra, dec;
        HealpixMath::pix2ang_nest(64, ipix, ra, dec);
        double dist = HealpixMath::angular_distance(0.0, 0.0, ra, dec);
        assert(dist <= 10.0 + 0.5);  // 容差 0.5°
    }
    printf("[PASS] test_query_disc (%zu ipix)\n", result.size());
}

void test_angular_distance() {
    // 相同点距离 0
    assert(approx(HealpixMath::angular_distance(0, 0, 0, 0), 0.0));
    // 对跖点距离 180
    assert(approx(HealpixMath::angular_distance(0, 90, 180, -90), 180.0));
    // 赤道上相距 90°
    assert(approx(HealpixMath::angular_distance(0, 0, 90, 0), 90.0));
    printf("[PASS] test_angular_distance\n");
}

int main() {
    test_pix2ang_nest_nside1();
    test_ang2pix_roundtrip();
    test_query_disc();
    test_angular_distance();
    printf("\n=== test_healpix_math: ALL PASS ===\n");
    return 0;
}
```

- [ ] **Step 3: 运行测试验证失败**

Run：
```powershell
cd "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt"
make tests/test_healpix_math.exe
```
Expected: 编译失败，`healpix_math.cpp` 不存在

- [ ] **Step 4: 写实现 core/healpix_math.cpp**

写入 `core/healpix_math.cpp`。实现 HEALPix NESTED 排序的 `pix2ang_nest`（三区域分块：北极/赤道/南极）、`ang2pix_nest`、`query_disc`（遍历过滤）、`angular_distance`（大圆距离）、`ud_grade`（NESTED 位运算降采样）。

**关键实现要点**：
- `npface = (uint64_t)nside * nside`
- `ip_low = ipix & (npface - 1)`（NESTED 低 2*log2(nside) 位）
- 北极区（`ipix < npface`）：`ix = ip_low & (nside-1)`, `iy = ip_low >> log2(nside)`, `dec = 90 - arcsin(3/2 - (iy+0.5)/nside) * 180/π`，ra 由 ix/iy 计算
- 赤道区（`npface <= ipix < 10*npface`）：四象限 + 左右唇
- 南极区（`ipix >= 10*npface`）：对称北极
- `ang2pix_nest`：先判断 dec 所在区域，再反推 ipix
- `angular_distance`：`arccos(sin(dec1)*sin(dec2) + cos(dec1)*cos(dec2)*cos(ra1-ra2)) * 180/π`

参考现有 `healpix_browser_web/js/webgl-renderer.js` 的 `pix2angNest` JS 实现逻辑，转写为 C++ `uint64_t` 精度版。

- [ ] **Step 5: 运行测试验证通过**

Run：
```powershell
cd "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt"
make tests/test_healpix_math.exe
.\tests\test_healpix_math.exe
```
Expected: `=== test_healpix_math: ALL PASS ===`

- [ ] **Step 6: 更新 memory.md，标记 Task 2 完成**

---

## Task 3: STFEngine（显示拉伸引擎）

**Files:**
- Create: `lib/healpix_db/healpix_browser_qt/core/stf_engine.h`
- Create: `lib/healpix_db/healpix_browser_qt/core/stf_engine.cpp`
- Test: `lib/healpix_db/healpix_browser_qt/tests/test_stf_engine.cpp`

**移植来源**：`healpix_browser_web/js/stf.js`。

- [ ] **Step 1: 写头文件**

写入 `core/stf_engine.h`（内容见 spec §3.2.1，完整接口定义：STFParams、GPUUniforms、STFEngine 类）。

- [ ] **Step 2: 写失败的测试**

写入 `tests/test_stf_engine.cpp`：
```cpp
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include "stf_engine.h"

static bool approx(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) < eps;
}

void test_mtf() {
    // MTF(0, m) = 0
    assert(approx(STFEngine::mtf(0.0f, 0.5f), 0.0f));
    // MTF(1, m) = 1
    assert(approx(STFEngine::mtf(1.0f, 0.5f), 1.0f));
    // MTF(m, m) = 0.5
    assert(approx(STFEngine::mtf(0.25f, 0.25f), 0.5f));
    // m=0.5 时线性：MTF(0.5, 0.5) = 0.5
    assert(approx(STFEngine::mtf(0.5f, 0.5f), 0.5f));
    printf("[PASS] test_mtf\n");
}

void test_presets() {
    auto linear = STFEngine::get_preset("linear");
    assert(approx(linear.midtones, 0.5f) && approx(linear.compression, 0.0f));

    auto sqrt_p = STFEngine::get_preset("sqrt");
    assert(approx(sqrt_p.midtones, 0.25f) && approx(sqrt_p.compression, 0.0f));

    auto asinh = STFEngine::get_preset("asinh");
    assert(approx(asinh.midtones, 0.25f) && approx(asinh.compression, 0.5f));

    auto log_p = STFEngine::get_preset("log");
    assert(approx(log_p.midtones, 0.15f) && approx(log_p.compression, 0.8f));
    printf("[PASS] test_presets\n");
}

void test_auto_stretch() {
    // 构造正态分布数据，median=50, sigma=10
    std::vector<float> data;
    for (int i = 0; i < 1000; ++i) {
        float x = 50.0f + 10.0f * (float)(i % 100 - 50) / 50.0f;
        data.push_back(x);
    }
    auto params = STFEngine::auto_stretch(data.data(), data.size(), 0.0f);
    // shadows 应 < median(50), highlights 应 > median
    assert(params.shadows < 50.0f);
    assert(params.highlights > 50.0f);
    assert(params.validate());
    printf("[PASS] test_auto_stretch (shadows=%.2f highlights=%.2f midtones=%.3f)\n",
           params.shadows, params.highlights, params.midtones);
}

void test_to_uniforms() {
    STFParams params;
    params.shadows = 10.0f;
    params.highlights = 100.0f;
    params.midtones = 0.5f;
    params.compression = 0.0f;

    auto u = STFEngine::to_uniforms(params, 0.0f, 100.0f, 0.0f);
    // 归一化：shadows=0.1, highlights=1.0
    assert(approx(u.shadows, 0.1f));
    assert(approx(u.highlights, 1.0f));
    printf("[PASS] test_to_uniforms\n");
}

int main() {
    test_mtf();
    test_presets();
    test_auto_stretch();
    test_to_uniforms();
    printf("\n=== test_stf_engine: ALL PASS ===\n");
    return 0;
}
```

- [ ] **Step 3: 运行测试验证失败**

Run：
```powershell
make tests/test_stf_engine.exe
```
Expected: 编译失败，`stf_engine.cpp` 不存在

- [ ] **Step 4: 写实现 core/stf_engine.cpp**

写入 `core/stf_engine.cpp`。实现：
- `mtf(x, m)`：`((m-1)*x) / ((2m-1)*x - m)`，处理 m=0.5 线性特例和 denom 近零
- `get_preset(name)`：linear/sqrt/asinh/log 四预设的 (midtones, compression)
- `auto_stretch(data, n)`：计算 median（nth_element）、MAD = median(|v-median|)、sigma = 1.4826*MAD、shadows = median - 3*sigma、highlights = median + 3*sigma、midtones = 归一化 median
- `to_uniforms(params, min, max)`：将原始像素值范围的 shadows/highlights 归一化到 [0,1]
- `STFParams::validate()`：检查 shadows < highlights，midtones ∈ (0,1)

**关键**：median 计算用 `std::nth_element`（O(n)），避免全排序。no_data 值过滤：计算统计量时跳过 `v <= no_data_value` 的像素。

- [ ] **Step 5: 运行测试验证通过**

Run：
```powershell
make tests/test_stf_engine.exe
.\tests\test_stf_engine.exe
```
Expected: `=== test_stf_engine: ALL PASS ===`

- [ ] **Step 6: 更新 memory.md，标记 Task 3 完成**

---

## Task 4: BrowserBackend（数据加载与按需子叶）

**Files:**
- Create: `lib/healpix_db/healpix_browser_qt/core/browser_backend.h`
- Create: `lib/healpix_db/healpix_browser_qt/core/browser_backend.cpp`
- Test: `lib/healpix_db/healpix_browser_qt/tests/test_browser_backend.cpp`

**移植来源**：`healpix_browser_cpp/src/browser_backend.cpp`，去掉 HTTP 部分。

- [ ] **Step 1: 写头文件**

写入 `core/browser_backend.h`（内容见 spec §3.1.1，完整接口定义：ViewParams、LeafData、BrowserBackend 类）。

- [ ] **Step 2: 写失败的测试**

写入 `tests/test_browser_backend.cpp`：
```cpp
#include <cassert>
#include <cstdio>
#include <string>
#include "browser_backend.h"

// 测试数据路径（使用现有 pipeline_debug 输出）
static const char* HISS_TEST = "f:/Astro dev/Astro CS Normalization Database/output/pipeline_debug/4_photometric/result_calibrated.hiss";
static const char* HCSD_TEST = "f:/Astro dev/Astro CS Normalization Database/output/pipeline_debug/6_drizzle/result_drizzle.hcsd";

void test_open_hiss() {
    BrowserBackend backend;
    int ret = backend.open_file(HISS_TEST);
    // 文件可能不存在，仅测试接口不崩溃
    if (ret != 0) {
        printf("[SKIP] test_open_hiss (测试文件不存在)\n");
        return;
    }
    assert(backend.is_open());
    assert(backend.is_hiss());
    assert(!backend.is_hcsd());
    assert(backend.get_nside() > 0);
    assert(backend.get_n_pix() > 0);
    backend.close_file();
    assert(!backend.is_open());
    printf("[PASS] test_open_hiss (nside=%u npix=%llu)\n",
           backend.get_nside(), (unsigned long long)backend.get_n_pix());
}

void test_open_hcsd() {
    BrowserBackend backend;
    int ret = backend.open_file(HCSD_TEST);
    if (ret != 0) {
        printf("[SKIP] test_open_hcsd (测试文件不存在)\n");
        return;
    }
    assert(backend.is_open());
    assert(backend.is_hcsd());
    assert(!backend.is_hiss());

    // 测试加载子叶 ipix=0
    ViewParams view;
    view.center_ra = 0;
    view.center_dec = 0;
    view.zoom = 1.0;
    view.fov_deg = 180.0;

    auto leaves = backend.get_required_leaves(view);
    assert(!leaves.empty());

    LeafData leaf = backend.load_leaf(leaves[0], 64);
    assert(leaf.n_pix > 0);
    assert(leaf.ipix != nullptr);
    assert(leaf.pixel != nullptr);
    backend.release_leaf(leaf);

    backend.close_file();
    printf("[PASS] test_open_hcsd (%zu leaves, first leaf npix=%llu)\n",
           leaves.size(), (unsigned long long)leaf.n_pix);
}

void test_ud_grade() {
    // 构造 nside=4 的 4 个像素，降采样到 nside=2
    LeafData input;
    input.leaf_ipix = 0;
    input.nside = 4;
    input.n_pix = 4;
    input.ipix = (uint64_t*)malloc(4 * sizeof(uint64_t));
    input.pixel = (float*)malloc(4 * sizeof(float));
    input.ipix[0] = 0; input.pixel[0] = 1.0f;
    input.ipix[1] = 1; input.pixel[1] = 2.0f;
    input.ipix[2] = 2; input.pixel[2] = 3.0f;
    input.ipix[3] = 3; input.pixel[3] = 4.0f;

    BrowserBackend backend;
    LeafData output = backend.ud_grade(input, 2);
    // nside=4 → nside=2，4 像素合并为 1，均值 = (1+2+3+4)/4 = 2.5
    assert(output.nside == 2);
    assert(output.n_pix == 1);
    assert(std::fabs(output.pixel[0] - 2.5f) < 0.01f);

    backend.release_leaf(input);
    backend.release_leaf(output);
    printf("[PASS] test_ud_grade\n");
}

int main() {
    test_open_hiss();
    test_open_hcsd();
    test_ud_grade();
    printf("\n=== test_browser_backend: ALL PASS ===\n");
    return 0;
}
```

- [ ] **Step 3: 运行测试验证失败**

Run：
```powershell
make tests/test_browser_backend.exe
```
Expected: 编译失败，`browser_backend.cpp` 不存在

- [ ] **Step 4: 写实现 core/browser_backend.cpp**

写入 `core/browser_backend.cpp`。实现：
- `open_file(path)`：调用 `hiss_read` 或 `hcsd_read`（通过文件头 magic 判断），填充 nside_/n_pix_/is_hiss_/filter_；.hiss 模式缓存 all_ipix_/all_pixel_
- `close_file()`：释放 all_ipix_/all_pixel_，重置状态
- `is_hiss()`/`is_hcsd()`：返回 is_hiss_ / !is_hiss_
- `get_required_leaves(view)`：根据 view.fov_deg 和 center 计算可见的 nside=64 子叶列表（用 HealpixMath::query_disc）
- `decide_target_nside(view, leaf_ipix)`：根据子叶距视角中心的角距离决定加载层级（spec §3.1.2 表格：中心 nside=8192，中间 2048，边缘 256）
- `load_leaf(leaf_ipix, target_nside)`：调用 `hcsd_read_leaf`，若 target_nside < 原始 nside 则调 `ud_grade` 降采样
- `ud_grade(input, target_nside)`：NESTED 位运算 `ipix_coarse = ipix_fine >> (2 * log2(src_nside / target_nside))`，4 相邻像素均值合并
- `get_all_data()`：返回 .hiss 缓存的全部数据
- `release_leaf(leaf)`：free ipix/pixel，置空
- `ipix_to_angle(nside, ipix, nested, ra, dec)`：转发 HealpixMath
- `angular_distance(...)`：转发 HealpixMath

**文件格式判断**：读文件头前 4 字节，".his" = hiss，".hcs" = hcsd。或尝试 hiss_read 失败再 hcsd_read。

**线程安全**：mutex_ 保护文件操作（与现有实现一致）。

- [ ] **Step 5: 运行测试验证通过**

Run：
```powershell
make tests/test_browser_backend.exe
.\tests\test_browser_backend.exe
```
Expected: `=== test_browser_backend: ALL PASS ===`（测试文件不存在时显示 SKIP）

- [ ] **Step 6: 编译 core 静态库验证**

Run：
```powershell
make core
```
Expected: 生成 `libhealpix_browser_core.a`（此时 gl_renderer.cpp 还不存在，会编译失败。本步在 Task 5 后执行，此处先跳过静态库编译，仅验证单独测试编译）

**调整**：由于 gl_renderer.cpp 尚未实现，Makefile 中 CORE_SRCS 暂时去掉 gl_renderer.cpp，Task 5 时加回。修改 Makefile 的 CORE_SRCS 行：
```makefile
CORE_SRCS = core/healpix_math.cpp core/stf_engine.cpp \
            core/browser_backend.cpp
```

- [ ] **Step 7: 更新 memory.md，标记 Task 4 完成**

---

## Task 5: GLRenderer（OpenGL 渲染核心）

**Files:**
- Create: `lib/healpix_db/healpix_browser_qt/core/gl_renderer.h`
- Create: `lib/healpix_db/healpix_browser_qt/core/gl_renderer.cpp`

**移植来源**：`healpix_browser_web/js/webgl-renderer.js` 的着色器、网格构建、绘制逻辑。WebGL 2.0 → OpenGL 3.3 Core。

**注意**：GLRenderer 需要 OpenGL 上下文才能运行，单元测试用 GLFW 创建测试窗口（可选）。本任务先实现编译通过的版本，实际渲染验证在 Task 12 集成测试时进行。

- [ ] **Step 1: 写头文件**

写入 `core/gl_renderer.h`（内容见 spec §3.4.1，完整接口定义：RenderMode、RenderParams、GLRenderer 类）。

- [ ] **Step 2: 写实现 core/gl_renderer.cpp**

写入 `core/gl_renderer.cpp`。实现：

**着色器编译**：
- `compile_shaders()`：编译球面顶点/片元着色器 + 单帧四边形顶点/片元着色器（源码见 spec §3.4.4），链接为 sphere_program_ / quad_program_

**网格构建**：
- `build_sphere_mesh(segments_lat=64, segments_lon=128)`：生成 UV 球面顶点（position vec3）+ 索引。顶点值（a_value）在 render 时动态填充，不在此处
- `build_quad_mesh()`：全屏四边形 (-1,-1)→(1,1)，含 texcoord

**球面渲染 render_sphere()**：
1. 根据 view.center_ra/dec/zoom 计算相机位置（球面外，距离 = R/zoom）
2. `backend.get_required_leaves(view)` → 需要的子叶
3. 对每个子叶：`backend.load_leaf(ipix, target_nside)` → 构建 ipix→value 的 Map
4. 球面网格 8192 顶点：每顶点 (ra,dec) → `HealpixMath::ang2pix_nest` → 查 Map → value
5. 上传顶点属性 (position + value) 到 VBO
6. 设置 MVP uniform + STF uniform（to_uniforms 转换后）
7. `glDrawElements`

**单帧渲染 render_single_frame()**：
1. 首次调用：`backend.get_all_data()` → 采样 1000 像素估算边界框（cos(dec) 修正 RA，10% 边距）→ `set_single_frame_bbox`
2. 视角变化检测：比较 view 与缓存，变化则重建 1024×1024 R32F 纹理
3. 纹理重建：每像素 TAN 逆投影 → (ra,dec) → ang2pix_nest → Map 查值，rho > π/2 跳过
4. 绑定 quad_program_，设置 STF uniform，绘制全屏四边形

**纹理管理**：
- `upload_leaf_texture(leaf)`：上传子叶数据为 1D 纹理（方案 A 备用）或直接填充顶点（方案 B 主用）
- `evict_unused_leaves(max=100)`：LRU 淘汰

**矩阵运算**：
- `perspective_matrix(fov, aspect, near, far)`：标准透视投影
- `look_at_matrix(eye, center, up)`：标准 look-at
- `multiply_matrix(a, b, out)`：4×4 矩阵乘法（column-major）

**STF uniform 更新**：
- `update_stf(stf, min, max)`：缓存参数，下次 render 时设置 uniform
- uniform 值通过 `STFEngine::to_uniforms(params, min, max, no_data)` 转换

**关键移植点**（WebGL → OpenGL 3.3）：
- `gl.createBuffer()` → `glGenBuffers`
- `gl.bindBuffer/gl.bufferData` → `glBindBuffer/glBufferData`
- `gl.createShader/gl.shaderSource` → `glCreateShader/glShaderSource`
- `attribute` → `layout(location=N) in`
- `varying` → `out`/`in`
- `gl_FragColor` → `out vec4 FragColor`
- WebGL 常量名（`gl.ARRAY_BUFFER`）→ OpenGL 常量（`GL_ARRAY_BUFFER`）

- [ ] **Step 3: 恢复 Makefile 中 CORE_SRCS 包含 gl_renderer.cpp**

修改 Makefile：
```makefile
CORE_SRCS = core/healpix_math.cpp core/stf_engine.cpp \
            core/browser_backend.cpp core/gl_renderer.cpp
```

- [ ] **Step 4: 编译 core 静态库**

Run：
```powershell
cd "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt"
make core
```
Expected: 生成 `libhealpix_browser_core.a`，无编译错误

**注意**：Windows 下 OpenGL 函数指针需手动加载（`wglGetProcAddress`）。实现时可用简单的方式：直接链接 `opengl32.lib`，核心函数（glGenBuffers 等 1.1 版本）直接可用，3.3 函数用 `wglGetProcAddress` 加载。或引入 glad/gl3w 加载器（单头文件）。

**简化方案**：用 `#include <windows.h>` + `#include <GL/gl.h>` 获取 1.1 函数，3.3 函数用 `wglGetProcAddress` 手动声明。或直接用 Qt 的 `QOpenGLFunctions_3_3_Core`（但 core/ 不能依赖 Qt）。

**最终方案**：core/ 内部用一个简单的 OpenGL 函数加载器（手写 50 行，或用 glad 预生成）。本任务在 gl_renderer.cpp 顶部内联一个 `init_gl_functions()` 函数，用 `wglGetProcAddress` 加载 3.3 函数指针。

- [ ] **Step 5: 写统一头文件**

写入 `include/healpix_browser_core.h`：
```cpp
#ifndef HEALPIX_BROWSER_CORE_H
#define HEALPIX_BROWSER_CORE_H

#include "browser_backend.h"
#include "stf_engine.h"
#include "healpix_math.h"
#include "gl_renderer.h"

#endif
```

- [ ] **Step 6: 更新 memory.md，标记 Task 5 完成 + core/ 完整**

---

## Task 6: AbstractView（Qt widget 基类）

**Files:**
- Create: `lib/healpix_db/healpix_browser_qt/widgets/abstract_view.h`
- Create: `lib/healpix_db/healpix_browser_qt/widgets/abstract_view.cpp`

**前置条件**：Qt6 已安装（MSYS2：`pacman -S mingw-w64-x86_64-qt6-base`）。core/ 静态库已编译。

- [ ] **Step 1: 验证 Qt6 环境**

Run：
```powershell
# 检查 Qt6 是否安装
ls C:\msys64\mingw64\lib\cmake\Qt6
```
Expected: 看到 Qt6Config.cmake。若不存在，提示用户安装：`pacman -S mingw-w64-x86_64-qt6-base`

- [ ] **Step 2: 写头文件**

写入 `widgets/abstract_view.h`（内容见 UI spec §3.1，完整接口定义：AbstractView 类继承 QOpenGLWidget + QOpenGLFunctions_3_3_Core）。

- [ ] **Step 3: 写实现 abstract_view.cpp**

写入 `widgets/abstract_view.cpp`。实现：
- 构造函数：初始化 backend_=nullptr, renderer_=nullptr, gl_initialized_=false
- `set_backend(backend)`：赋值 backend_，触发 update()
- `set_stf_params(params)`：stf_params_ = params，触发 update()
- `auto_stretch()`：从 backend_ 取数据（.hiss 全量 / .hcsd 前几个子叶采样），调 `STFEngine::auto_stretch` 计算 params，同时算 data_min/max
- `initializeGL()`：`initializeOpenGLFunctions()` → `renderer_ = make_unique<GLRenderer>()` → `renderer_->init()` → `compute_data_range()`
- `resizeGL(w,h)`：空（renderer 用 viewport 参数）
- `paintGL()`：`RenderParams p = build_render_params()` → `renderer_->render(*backend_, p)`
- `mousePressEvent/mouseMoveEvent/mouseReleaseEvent/wheelEvent`（final）：转发到 `handle_*` 纯虚函数
- `compute_data_range()`：遍历 backend 数据采样求 min/max（.hiss 全量遍历；.hcsd 加载前 10 个子叶采样）
- 析构函数：`makeCurrent()` → `renderer_->cleanup()` → `doneCurrent()`

- [ ] **Step 4: 写 CMakeLists.txt 骨架（widgets 部分）**

写入 `CMakeLists.txt`（完整版见 UI spec §7.1）。先写 core + widgets 部分，app 部分在 Task 9 补全。

- [ ] **Step 5: 验证编译**

Run：
```powershell
cd "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt"
mkdir build 2>$null
cd build
cmake -DCMAKE_PREFIX_PATH="C:/msys64/mingw64" ..
cmake --build . --target healpix_browser_qt_widgets
```
Expected: 编译 `libhealpix_browser_qt_widgets.a` 成功（abstract_view.cpp 编译通过）

- [ ] **Step 6: 更新 memory.md，标记 Task 6 完成**

---

## Task 7: SingleFrameView（单帧 2D 切面投影）

**Files:**
- Create: `lib/healpix_db/healpix_browser_qt/widgets/single_frame_view.h`
- Create: `lib/healpix_db/healpix_browser_qt/widgets/single_frame_view.cpp`

- [ ] **Step 1: 写头文件**

写入 `widgets/single_frame_view.h`（内容见 UI spec §3.2，完整接口定义：SingleFrameView 类）。

- [ ] **Step 2: 写实现**

写入 `widgets/single_frame_view.cpp`。实现：
- 构造函数：center_ra_/dec_=0, zoom_=1.0, is_dragging_=false, data_fov_deg_=10.0
- `init_view_from_data()`：从 backend_ 采样像素估算数据边界框，设置 center_ra_/dec_ 为数据中心，data_fov_deg_ 为数据 FOV
- `get_view_params(out)`：填充 out
- `handle_mouse_press`：记录 last_mouse_x/y, is_dragging=true
- `handle_mouse_move`：`delta_ra = -dx * drag_sensitivity * cos(dec)`, `delta_dec = dy * drag_sensitivity`，更新 center_ra_/dec_，emit view_changed, update()
- `handle_mouse_release`：is_dragging=false
- `handle_wheel`：`zoom_ *= exp(-delta * 0.001)`，clamp [0.5, 100]，emit view_changed, update()
- `build_render_params()`：构造 RenderParams（mode=SINGLE_FRAME, view, stf, data_min/max, viewport_w/h）
- `drag_sensitivity()`：`data_fov_deg_ / (zoom_ * width())`
- `view_changed_since_last_render()`：比较当前视角与上次渲染值

- [ ] **Step 3: 更新 CMakeLists.txt 加入 single_frame_view.cpp**

- [ ] **Step 4: 验证编译**

Run：
```powershell
cd build
cmake --build . --target healpix_browser_qt_widgets
```
Expected: 编译成功

- [ ] **Step 5: 更新 memory.md，标记 Task 7 完成**

---

## Task 8: SphereView（球面 3D 渲染）

**Files:**
- Create: `lib/healpix_db/healpix_browser_qt/widgets/sphere_view.h`
- Create: `lib/healpix_db/healpix_browser_qt/widgets/sphere_view.cpp`

- [ ] **Step 1: 写头文件**

写入 `widgets/sphere_view.h`（内容见 UI spec §3.3，完整接口定义：SphereView 类）。

- [ ] **Step 2: 写实现**

写入 `widgets/sphere_view.cpp`。实现：
- 构造函数：center_ra_/dec_=0, zoom_=1.0, fov_deg_=180.0, is_dragging_=false
- `get_view_params(out)`：填充 out
- `reset_view()`：center_ra_/dec_=0, zoom_=1.0
- `handle_mouse_press`：记录 last_mouse_x/y, is_dragging=true
- `handle_mouse_move`：`center_ra_ -= dx * ROTATE_SPEED`, `center_dec_ += dy * ROTATE_SPEED`（clamp [-90,90]），emit view_changed + mouse_moved(screen_to_sky), update()
- `handle_mouse_release`：is_dragging=false
- `handle_wheel`：`zoom_ *= exp(-delta * ZOOM_SPEED)`, `fov_deg_ = 180/zoom_`，clamp zoom [0.5,100], update()
- `build_render_params()`：构造 RenderParams（mode=SPHERE, view, stf, viewport）
- `screen_to_sky(x, y, ra, dec)`：球面投影逆变换（屏幕坐标 → 球面坐标）
- 触摸事件处理（单指拖动 + 双指捏合）

- [ ] **Step 3: 更新 CMakeLists.txt 加入 sphere_view.cpp**

- [ ] **Step 4: 验证编译**

Run：
```powershell
cd build
cmake --build . --target healpix_browser_qt_widgets
```
Expected: 编译成功

- [ ] **Step 5: 更新 memory.md，标记 Task 8 完成 + widgets/ 完整**

---

## Task 9: MainWindow（主窗口）

**Files:**
- Create: `lib/healpix_db/healpix_browser_qt/app/main_window.h`
- Create: `lib/healpix_db/healpix_browser_qt/app/main_window.cpp`

- [ ] **Step 1: 写头文件**

写入 `app/main_window.h`（内容见 UI spec §4.1，完整接口定义：MainWindow 类含 public slot open_file_from_cli）。

- [ ] **Step 2: 写实现**

写入 `app/main_window.cpp`。实现：
- 构造函数：`setup_menu()`, `setup_status_bar()`, `backend_ = make_unique<BrowserBackend>()`, `current_view_=nullptr`
- `setup_menu()`：File > Open / Close / Exit，connect 到对应 slot
- `setup_status_bar()`：3 个 QLabel（status_file_, status_view_, status_mouse_）
- `on_file_open()`：`QFileDialog::getOpenFileName` → `open_file(path)`
- `open_file_from_cli(path)`：public slot，调用 `open_file(path)`
- `open_file(path)`：`backend_->open_file` → 按 is_hiss/is_hcsd 创建 SingleFrameView/SphereView → `set_backend` → `auto_stretch` → `set_view` → connect 信号槽
- `close_file()`：删除 current_view_，backend_->close_file()
- `set_view(view)`：将 view 设为 centralWidget
- `on_stf_changed(params)`：`current_view_->set_stf_params(params)`
- `on_view_changed(ra, dec, zoom)`：更新 status_view_ 文本
- `on_mouse_moved(ra, dec)`：更新 status_mouse_ 文本
- `on_file_close()`：`close_file()`
- `on_exit()`：`close()`
- 析构函数：`close_file()`

- [ ] **Step 3: 更新 CMakeLists.txt 加入 main_window.cpp + app exe target**

- [ ] **Step 4: 验证编译**

Run：
```powershell
cd build
cmake --build . --target healpix_browser_qt
```
Expected: 编译失败（main.cpp 不存在，Task 10 补全）。此步仅验证 main_window.cpp 编译通过。

**调整**：先写 main.cpp 骨架（Task 10 Step 1），再编译。

- [ ] **Step 5: 更新 memory.md，标记 Task 9 完成**

---

## Task 10: STFPanel + main.cpp（控制面板与入口）

**Files:**
- Create: `lib/healpix_db/healpix_browser_qt/app/stf_panel.h`
- Create: `lib/healpix_db/healpix_browser_qt/app/stf_panel.cpp`
- Create: `lib/healpix_db/healpix_browser_qt/app/main.cpp`

- [ ] **Step 1: 写 main.cpp**

写入 `app/main.cpp`（内容见 UI spec §4.3）。

- [ ] **Step 2: 写 stf_panel.h**

写入 `app/stf_panel.h`（内容见 UI spec §4.2，STFPanel 类）。

- [ ] **Step 3: 写 stf_panel.cpp**

写入 `app/stf_panel.cpp`。实现：
- 构造函数：`setup_ui()`
- `setup_ui()`：创建 QComboBox(preset) + 4 个 QSlider(shadows/highlights/midtones/compression) + QPushButton(auto)，垂直布局，connect 信号
- `on_slider_changed()`：`collect_params()` → `emit stf_changed(params)`
- `on_preset_clicked()`：`STFEngine::get_preset(name)` → `update_sliders(params)` → `emit stf_changed`
- `on_auto_stretch_clicked()`：emit 特殊信号通知 MainWindow 调用 view->auto_stretch()
- `update_sliders(params)`：反向映射 float→int 填充滑块
- `collect_params()`：从 4 个滑块值映射回 STFParams

- [ ] **Step 4: 完整 CMakeLists.txt**

确认 CMakeLists.txt 包含 app exe target（main.cpp + main_window.cpp + stf_panel.cpp），链接 widgets + core + Qt6::Widgets。

- [ ] **Step 5: 编译完整 exe**

Run：
```powershell
cd "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt\build"
cmake --build .
```
Expected: 生成 `healpix_browser_qt.exe`

- [ ] **Step 6: 更新 memory.md，标记 Task 10 完成 + app/ 完整**

---

## Task 11: 运行 demo 验证功能

**Files:**
- 无新文件，验证现有实现

- [ ] **Step 1: 准备测试数据**

检查 `output/pipeline_debug/` 下是否有 .hiss / .hcsd 文件。若无，运行 pipeline_debug 生成。

Run：
```powershell
ls "f:\Astro dev\Astro CS Normalization Database\output\pipeline_debug\4_photometric\*.hiss"
ls "f:\Astro dev\Astro CS Normalization Database\output\pipeline_debug\6_drizzle\*.hcsd"
```

- [ ] **Step 2: 复制依赖 DLL**

Run：
```powershell
$src = "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_io\healpix_io.dll"
$dst = "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt\build"
Copy-Item $src $dst
```

- [ ] **Step 3: 运行 demo 打开 .hiss 文件**

Run：
```powershell
cd "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt\build"
.\healpix_browser_qt.exe "f:\Astro dev\Astro CS Normalization Database\output\pipeline_debug\4_photometric\result_calibrated.hiss"
```
Expected: 窗口打开，显示单帧切面投影图像，可拖动平移、滚轮缩放，STF 滑块实时调整拉伸

- [ ] **Step 4: 运行 demo 打开 .hcsd 文件**

Run：
```powershell
.\healpix_browser_qt.exe "f:\Astro dev\Astro CS Normalization Database\output\pipeline_debug\6_drizzle\result_drizzle.hcsd"
```
Expected: 窗口打开，显示球面渲染，可拖动旋转、滚轮缩放，子叶按需加载

- [ ] **Step 5: 验证 STF 功能**

在 UI 中测试：
- 4 个预设下拉框（linear/sqrt/asinh/log）切换
- 自动拉伸按钮
- 4 个滑块手动调整

Expected: 渲染实时更新

- [ ] **Step 6: 验证状态栏**

Expected: 状态栏显示文件名、视角坐标（RA/Dec/zoom）、鼠标坐标

- [ ] **Step 7: 更新 memory.md，记录验证结果**

---

## Task 12: 性能验证

**Files:**
- 无新文件，性能测量

- [ ] **Step 1: 在 GLRenderer::render 中添加计时日志**

修改 `core/gl_renderer.cpp`，在 render 入口和出口添加：
```cpp
#include "logger.h"
#include <chrono>

int GLRenderer::render(BrowserBackend& backend, const RenderParams& params) {
    auto t0 = std::chrono::high_resolution_clock::now();
    // ... 渲染逻辑 ...
    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    LOG_DEBUG("render: %.2f ms (mode=%d)", ms, (int)params.mode);
    return 0;
}
```

- [ ] **Step 2: 重新编译并运行**

Run：
```powershell
cd build
cmake --build .
.\healpix_browser_qt.exe "<hcsd 文件>"
```

- [ ] **Step 3: 收集性能数据**

操作球面浏览器：拖动旋转 10 秒，观察 stderr 日志中的 render 时间。
Expected:
- 球面单帧渲染 < 16ms（60fps）
- 视角变化响应 < 50ms

- [ ] **Step 4: 与 WebGL 基线对比**

在 memory.md 记录：
- WebGL 基线：球面单帧 ~50-100ms（HTTP 往返主导）
- C++ 新版：球面单帧 < 16ms（目标）

- [ ] **Step 5: 更新 memory.md，记录性能数据**

---

## Task 13: 归档 WebGL 浏览器 + 更新文档

**Files:**
- Move: `lib/healpix_db/healpix_browser_cpp/` → `lib/healpix_db/archive/healpix_browser_cpp/`
- Move: `lib/healpix_db/healpix_browser_web/` → `lib/healpix_db/archive/healpix_browser_web/`
- Create: `lib/healpix_db/archive/ARCHIVE_INDEX.md`
- Modify: `PROJECT_ARCHITECTURE.md`（模块清单更新）
- Modify: `lib/healpix_db/memory.md`（若存在，记录架构变更）

- [ ] **Step 1: 创建 archive 目录**

Run：
```powershell
$base = "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db"
New-Item -ItemType Directory -Force -Path "$base\archive"
```

- [ ] **Step 2: 移动 healpix_browser_cpp 到 archive**

Run：
```powershell
$base = "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db"
Move-Item "$base\healpix_browser_cpp" "$base\archive\healpix_browser_cpp"
```
Expected: 目录移动成功

- [ ] **Step 3: 移动 healpix_browser_web 到 archive**

Run：
```powershell
$base = "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db"
Move-Item "$base\healpix_browser_web" "$base\archive\healpix_browser_web"
```
Expected: 目录移动成功

- [ ] **Step 4: 写 ARCHIVE_INDEX.md**

写入 `lib/healpix_db/archive/ARCHIVE_INDEX.md`：
```markdown
# 归档索引

## healpix_browser_cpp
- 归档日期：2026-07-13
- 归档原因：被 healpix_browser_qt 替代（HTTP + base64 通讯开销过大）
- 原功能：C++ 后端 + winsock2 HTTP 服务器（localhost:18080）
- 替代模块：../healpix_browser_qt/

## healpix_browser_web
- 归档日期：2026-07-13
- 归档原因：被 healpix_browser_qt 替代（WebGL 通讯开销 + 无法嵌入）
- 原功能：WebGL 前端（球面渲染 + STF + 视角交互）
- 替代模块：../healpix_browser_qt/
```

- [ ] **Step 5: 更新 PROJECT_ARCHITECTURE.md 模块清单**

修改 `PROJECT_ARCHITECTURE.md` §2.1 模块清单：
- `healpix_browser_cpp` 标记为"已归档（archive/）"
- `healpix_browser_web` 标记为"已归档（archive/）"
- 新增 `healpix_browser_qt` 模块描述（C++ + Qt6，替代上述两个模块）

- [ ] **Step 6: 更新 lib/healpix_db/memory.md（若存在）**

检查 `lib/healpix_db/memory.md` 是否存在，若存在则追加架构变更记录。

- [ ] **Step 7: 更新根 memory.md**

修改根 `memory.md`，在模块索引中新增 `lib/healpix_db/healpix_browser_qt/`，标记旧模块归档。

- [ ] **Step 8: 最终验证**

Run：
```powershell
# 确认归档后 demo 仍可运行
cd "f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_browser_qt\build"
.\healpix_browser_qt.exe
```
Expected: 程序正常启动（File > Open 可用）

- [ ] **Step 9: 更新模块 memory.md，标记全部完成**

修改 `lib/healpix_db/healpix_browser_qt/memory.md`，所有 Task 标记 [x]，记录最终状态。

---

## Self-Review 检查

**Spec 覆盖检查**：
- ✅ core/ 四个组件（HealpixMath/STFEngine/BrowserBackend/GLRenderer）→ Task 2-5
- ✅ widgets/ 三个类（AbstractView/SingleFrameView/SphereView）→ Task 6-8
- ✅ app/ 三个文件（MainWindow/STFPanel/main）→ Task 9-10
- ✅ UI 改进（独立入口 + 文件选择 UI）→ Task 9（文件路由）+ Task 10（QFileDialog）
- ✅ 归档计划 → Task 13
- ✅ 性能验证 → Task 12
- ✅ 对外头文件 healpix_browser_core.h → Task 5 Step 5
- ✅ CMakeLists.txt → Task 6 Step 4 + Task 10 Step 4
- ✅ 测试策略 → Task 2-4 单元测试 + Task 11 集成测试

**类型一致性检查**：
- ✅ BrowserBackend 接口在 Task 4 定义，Task 6（AbstractView）和 Task 9（MainWindow）使用一致
- ✅ GLRenderer 接口在 Task 5 定义，Task 6-8 使用一致
- ✅ STFParams 在 Task 3 定义，Task 6（auto_stretch）、Task 10（STFPanel）使用一致
- ✅ ViewParams 在 Task 4（browser_backend.h）定义，Task 7-8 使用一致
- ✅ RenderParams 在 Task 5（gl_renderer.h）定义，Task 7-8（build_render_params）使用一致

**占位符扫描**：无 TBD/TODO，所有步骤有具体代码或具体实现指引。

---

## 执行选择

**Plan complete and saved to `docs/superpowers/plans/2026-07-13-cpp-qt-browser.md`. Two execution options:**

**1. Subagent-Driven (recommended)** - 每个 Task 分派独立 subagent，任务间审查，快速迭代

**2. Inline Execution** - 在当前会话批量执行，带检查点审查

**Which approach?**
