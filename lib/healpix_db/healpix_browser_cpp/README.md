# healpix_browser_cpp

C++ 渲染后端 + HTTP 服务器，配合 [healpix_browser_web](../healpix_browser_web/) 前端实现基于 WebGL 的 HEALPix 球面浏览器。

## 功能

- 管理 `.hiss` / `.hcsd` 文件
- 按需子叶加载 (`hcsd_read_leaf`)
- 视角相关压缩（中心高分辨率 8192，中间 2048，边缘 256）
- `ud_grade` 降采样（NESTED 排序下 4 相邻像素合并求均值）
- HTTP 服务器提供前端页面 + 数据 API

## 用法

```powershell
.\browser_cpp.exe <file.hiss|file.hcsd>
```

程序启动后自动打开默认浏览器访问 `http://localhost:18080`。

## 编译

```powershell
# 在本目录下执行
powershell -ExecutionPolicy Bypass -File build.ps1
```

或用 Makefile（需要 MSYS2/MinGW64 在 PATH）：

```powershell
make
```

构建脚本会自动复制 `healpix_io.dll` 到当前目录。

## HTTP API

| 路径 | 方法 | 参数 | 说明 |
|------|------|------|------|
| `/` | GET | - | 返回 `index.html` |
| `/api/file_info` | GET | - | 返回 `{is_hiss, is_hcsd, nside, n_pix, file_path}` |
| `/api/required_leaves` | GET | `ra`, `dec`, `zoom`, `fov` | 返回 `{leaves:[...], count}` |
| `/api/leaf` | GET | `ipix`, `nside` | 返回 `{leaf_ipix, n_pix, nside, ipix:[...], pixel:[...]}` |
| `/api/all_data` | GET | - | 返回全量数据（仅 `.hiss` 模式） |

## 文件结构

```
healpix_browser_cpp/
├── include/
│   ├── browser_backend.h    # BrowserBackend 类定义
│   └── http_server.h        # HttpServer 类定义
├── src/
│   ├── browser_backend.cpp  # 后端实现 (文件管理/子叶加载/ud_grade)
│   ├── http_server.cpp      # HTTP 服务器实现 (winsock2)
│   └── browser_main.cpp     # 入口函数
├── Makefile                 # Makefile (MinGW)
├── build.ps1                # PowerShell 构建脚本
└── README.md
```

## 依赖

- **healpix_io.dll**（位于 `../healpix_io/`）：提供 `hiss_read` / `hcsd_read` / `hcsd_read_leaf` / `hio_free`
- **winsock2**（Windows 系统库，`-lws2_32`）：HTTP 服务器网络通信
- **MSYS2/MinGW64**：编译环境

## 设计要点

### 视角相关压缩

| 子叶与视角中心距离 | 目标 nside | 说明 |
|--------------------|-----------|------|
| `< fov/4` | 8192 | 中心区域，全分辨率 |
| `< fov/2` | 2048 | 中间区域，中等降采样 |
| `>= fov/2` | 256 | 边缘区域，高强度压缩 |

### ud_grade 降采样（NESTED 排序）

- 4 个相邻像素合并：`ipix_coarse = ipix_fine >> 2`
- 按 `ipix_coarse` 分组求均值
- 支持任意 2 的幂次降采样

### 静态文件服务

- 静态文件根目录：`../healpix_browser_web/`
- 自动根据扩展名设置 `Content-Type`
- 路径遍历防护（拒绝 `..` 路径）

## 相关模块

- [healpix_io](../healpix_io/) - HEALPix 文件读写
- [healpix_browser_web](../healpix_browser_web/) - WebGL 前端
