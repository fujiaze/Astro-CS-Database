# P13-002 测试报告

## 测试范围
本报告覆盖 P13-002 会话期间的所有验证测试。

## 1. 栈溢出修复验证

### 测试方法
对 3 个之前栈溢出失败的帧单独运行 stage1。

### 测试结果
| 帧名 | 滤镜 | 修复前 | 修复后 | HISS 大小 |
|------|------|--------|--------|-----------|
| Victory_Nebula_mosaic1@045711-Red | RED | exit=3221225725 | exit=0 | 85729B |
| Victory_Nebula_mosaic1@060603-Blue | BLUE | exit=3221225725 | exit=0 | 86155B |
| Victory_Nebula_mosaic1@071753-Blue | BLUE | exit=3221225725 | exit=0 | 87407B |

**结论**: 3/3 PASS

## 2. 浏览器 CLI 工具验证

### DLL 依赖诊断
```
astro_image_io.dll        OK
libgcc_s_seh-1.dll        OK
libstdc++-6.dll           OK
libwinpthread-1.dll       OK
Qt6Core.dll               OK
Qt6Gui.dll                OK
Qt6Widgets.dll            OK
Qt6OpenGL.dll             OK
Qt6OpenGLWidgets.dll      OK
```
**结论**: 9/9 DLL OK

### .hiss 加载性能
- 文件: T4_RED_Galaxy_Center.hiss
- 加载时间: 3.4ms
- nside: 512, n_pix: 3928
- get_all_data: 0.0ms
- ud_grade(->64): 0.2ms
- 内存: 8MB

### .hcsd 球面模式性能
- 文件: galaxy_center_stacked.hcsd
- 加载时间: 1.4ms
- nside: 512, n_pix: 4094
- get_required_leaves: 17.0ms (7502 个子叶)
- avg_load_leaf: 0.42ms/叶
- 模拟缩放: 55.2 FPS, 18.1ms/帧

### 胜利 LUM 叠加结果性能
- 文件: victory_lum_stacked.hcsd
- 加载时间: 1.2ms
- nside: 512, n_pix: 6890
- 模拟缩放: 63.4 FPS, 15.8ms/帧

## 3. Stage2 验证

### 银心 5 代表帧
```
输入: T4_RED/GREEN/BLUE/HA/OIII_Galaxy_Center.hiss
输出: galaxy_center_stacked.hcsd (1.2MB)
GRADIENT_SPHERE: 0.017s success=true
STACK: 骨架跳过
```

### 胜利 20 帧 LUM
```
输入: 20 帧 Victory_Nebula LUM .hiss
输出: victory_lum_stacked.hcsd (6890 像素)
GRADIENT_SPHERE: 0.063s success=true
STACK: 骨架跳过
```

## 4. 浏览器部署验证
- windeployqt 部署 Qt6 DLL 和插件
- 复制 libgomp-1.dll + liblz4.dll
- 双击启动验证: 成功（PID=42508）

## 测试汇总
| 测试项 | 结果 |
|--------|------|
| 栈溢出修复 | 3/3 PASS |
| DLL 诊断 | 9/9 OK |
| .hiss 加载 | PASS |
| .hcsd 球面模式 | PASS (55-63 FPS) |
| Stage2 银心 | PASS |
| Stage2 胜利 LUM | PASS |
| 浏览器部署 | PASS |
