# 银心三片 Red 马赛克正式数据集

冻结 panel1 Red 11、panel2 Red 11、panel3 Red 10，共 32 帧。Agent 从 TestData 文档与 Header 自动确认路径、设备、滤镜和面板，不在 Spec 中硬编码绝对路径。

阶段：

1. 每片各 1 帧最小连通性；
2. 32 帧全部 HISS；
3. 梯度关闭 HCSD；
4. 梯度开启 HCSD；
5. 已知非零球面梯度注入与恢复；
6. 浏览器相同视角/同 STF 比较。

重叠图必须包含 panel1↔panel2 和 panel2↔panel3，整体连通。panel1↔panel3 可不直接重叠。
