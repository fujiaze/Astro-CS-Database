# 球面浏览器检查规范

浏览器是科学检查工具，优先正确显示 HISS/HCSD，而非当前主产品。

必须支持：signal、SNR控制点、support、总曲面、每帧校正面、控制点、拟合残差、coverage、rejection、total weight、gradient confidence、seam overlay。

性能优化顺序：header/index-only → 持久句柄 → batch leaf read → 后台I/O/解压 → 请求取消 → CPU/GPU LRU → 交互低LOD → GPU Tile Renderer。性能只用真实Qt/OpenGL窗口和真实文件测量。
