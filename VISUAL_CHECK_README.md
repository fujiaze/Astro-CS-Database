# AstroCS HiPS 浏览器 — 用户视觉验收（1–2 分钟）

## 1. 启动

在项目根目录打开 PowerShell 7，运行：

```powershell
pwsh -File .\launch\start_browser.ps1
```

脚本会自动打开浏览器并默认加载 **Galactic Center 三 panel Red** 真实 mosaic。
（第一次会自动构建浏览器，需要几分钟；之后秒开。）

如果想打开其他产品：

```powershell
pwsh -File .\launch\start_browser.ps1 -HipsPath run\phase2\v9\geometry_truth.hips
```

## 2. 正常情况下会看到什么

- 窗口标题：`HEALPix Browser (Qt)`；
- 中央是银心三 panel 的天区图像（三块亮区纵向排列，周围深色背景）；
- 窗口底部状态栏显示：文件/order、视角（RA/Dec/FOV）、鼠标所指坐标；
- 右下角有预设下拉框。

## 3. Signal / Support 切换

- 工具栏有一个 `Support` 开关按钮：按下显示覆盖度（灰阶，0 覆盖为黑），
  松开显示 Signal（天文亮度，默认 asinh 拉伸）。

## 4. 跳到 Wide / overlap / seam

右下角预设下拉框：

- `GC Wide` — 整个三 panel；
- `Overlap 1-2` — panel1/panel2 重叠带；
- `Overlap 2-3` — panel2/panel3 重叠带；
- `Seam Close-up` — 接缝放大；
- `Support View` — 覆盖度视图。

（打开几何 truth 产品时预设变为 Equator / Wrap / Polar / Multi-face。）

## 5. 鼠标操作

- 左键按住拖动 = 平移；
- 滚轮 = 缩放（也可以点工具栏 🔍+/🔍-）；
- 底部状态栏实时显示鼠标所指 RA/Dec。

## 6. 请重点检查

1. 默认看到的是 GC 三 panel（不是黑屏/占位图）；
2. Wide 视图三 panel 相对位置合理；
3. Overlap 1-2 / 2-3 没有明显人为亮暗台阶；
4. Seam Close-up 放大后没有固定 tile 网格裂缝/错位；
5. 银河结构/亮点随缩放不跳位、不镜像、不旋转；
6. Support 视图与覆盖区域一致，空覆盖区正确；
7. 连续平移缩放 5 分钟不卡死、不无限吃内存。

## 7. 退出

- 直接关浏览器窗口；或回到终端按 `Ctrl+C`（脚本会自动清理进程）。

---

结论请回填给 Agent：

```text
USER_VISUAL_ACCEPTANCE=PASS
```

或指出具体问题（例如 `FAIL: zoom 时星点跳半个 tile`），下一轮只修你看到的问题。
