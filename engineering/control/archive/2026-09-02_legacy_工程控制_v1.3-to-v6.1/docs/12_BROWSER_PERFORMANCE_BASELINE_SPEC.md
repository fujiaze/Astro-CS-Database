# 球面浏览器性能基线

在修改前记录固定硬件、当前银心 HCSD、窗口大小、视角、FOV、STF、冷/热缓存。指标包括：open metadata、首帧、首个高分辨率视图、拖动/缩放 frame time p50/p95/p99、GUI 最长阻塞、磁盘吞吐、CPU/GPU、峰值内存、叶请求数和缓存命中。

加入 trace 点：文件打开、Header 解压、索引读取、可见叶查询、leaf I/O、ud_grade、CPU lookup、VBO 构建/上传、draw。基线必须证明瓶颈位置，不能仅报告“很卡”。
