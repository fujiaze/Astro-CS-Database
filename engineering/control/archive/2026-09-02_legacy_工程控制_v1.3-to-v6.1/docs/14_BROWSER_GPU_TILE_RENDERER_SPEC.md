# GPU Tile Renderer 规范

替换当前“每帧 CPU 对百万顶点查 HEALPix 值并重传 VBO”的架构。

Tile key 至少含 dataset、leaf、LOD、channel。每个可见 Tile 的科学值保持 R32F（可选经证据使用 R16F），上传后相机移动只更新矩阵；STF 在 fragment shader 中完成。几何模板复用，GPU 资源由渲染线程创建/销毁。

要求：

- 不再每帧构建整球 vertex_data；
- 不再把数据转 uint8；
- 缺失高 LOD 时显示低 LOD，不阻塞；
- Tile 边界无明显裂缝/接缝；
- 数值采样与 CPU reference 在容差内一致。
