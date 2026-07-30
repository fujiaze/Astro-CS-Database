# -*- coding: utf-8 -*-
"""g_chain - Gate G (G-001~G-003) + F-002 实现 包。

子模块:
  g_common         - 公共约定 (数据结构, E-003 结果加载, HCSD 纯 Python 读写器, 日志)
  g001_reject      - G-001 梯度校正后稳健排异 (加性偏移校正 + MAD 异常检测)
  g002_fusion      - G-002 独立 SNR^2 连续加权融合
  g003_hcsd        - G-003 HCSD 生产层 + 可开关调试质量层
  run_g_pipeline   - G-001~G-003 运行脚本
  f002_verify      - F-002 三片最小总曲面 / HCSD / 浏览器检查
  run_f002         - F-002 运行脚本
"""
