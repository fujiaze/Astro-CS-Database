# -*- coding: utf-8 -*-
"""e_chain - Gate E 加性共识曲面 pipeline (E-001 ~ E-004).

模块:
  e_common          公共约定 (数据结构, HISS 读取, 曲面基底, IDW)
  e_masks_sampling  E-001 掩膜 + 稀疏控制点采样
  e_weights         E-002 SNR^2/逆方差联合权重 + 重叠区共识
  e_solver          E-003 全局加性共识曲面稀疏求解器
  e_injection_test  E-004 已知梯度/SNR/异常注入恢复测试
"""
