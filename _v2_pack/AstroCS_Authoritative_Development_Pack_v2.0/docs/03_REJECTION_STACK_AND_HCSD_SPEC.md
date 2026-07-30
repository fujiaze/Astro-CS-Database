# 排异、叠加与 HCSD 规范

## 顺序

1. 应用每帧加性校正面；
2. 在每个目标球面像素收集样本；
3. 使用加权中位数、weighted MAD、Huber/Tukey或小样本规则排异；
4. 对保留样本独立计算 SNR² × support × edge × gradient-confidence 权重；
5. 归一化融合；
6. 无有效权重时输出无数据，不输出零亮度。

## 调试质量层

`--debug-quality-layers` 默认关闭。开启后保存 coverage count、rejected count、total weight、gradient confidence、global target surface、per-frame correction、control points、residual、seam diagnostics、support。

## 连续性 Gate

不得出现面板边界跳变、覆盖数变化块状断层、权重硬切换、无数据零值污染和梯度外推断层。
