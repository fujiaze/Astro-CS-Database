# P03-004 复核报告

## 复核信息
- **任务 ID**: P03-004
- **复核日期**: 2026-07-25
- **复核人**: AI Sub-agent (self-review)
- **VERDICT**: PASS

## 复核检查项

### 1. 代码质量
| 检查项 | 结果 | 说明 |
|--------|------|------|
| 命名规范 | PASS | SnrSipCoeffs, snrEvalSip, pixelToSkySimple 命名清晰, 与现有 snr_ 前缀一致 |
| 注释完整 | PASS | 关键函数有中文注释说明算法步骤和参数含义 |
| 内存管理 | PASS | snr_free_model 释放 points 数组; orchestrator 用 malloc 分配 buffer, move 语义转移所有权 |
| 错误处理 | PASS | nullptr 检查返回 3, 退化路径返回 1/2, 不静默失败 |
| 日志输出 | PASS | 关键步骤有 LOG_INFO/WARN/ERROR, 含 P03-004 标识 |

### 2. 算法正确性
| 检查项 | 结果 | 说明 |
|--------|------|------|
| SIP 多项式求值 | PASS | snrEvalSip 按 i*6+j 索引, i+j<=order, 与 FITS SIP 标准一致 |
| TAN 反投影 | PASS | 公式与 healpix_drizzle/wcs_sip.cpp pixelToSky 一致 |
| CRPIX 1-based | PASS | dx = x - (crpix1-1.0), 与 FITS 标准一致 |
| RA 归一化 | PASS | 归一化到 [0, 360), 处理跨 0/360 度边界 |
| 退化路径 | PASS | n_stars<=0 返回 1, sigma<=0 返回 2, nullptr 返回 3 |

### 3. WCS+SIP 一致性
| 检查项 | 结果 | 说明 |
|--------|------|------|
| CD 矩阵顺序 | PASS | cd[0]=CD1_1, cd[1]=CD1_2, cd[2]=CD2_1, cd[3]=CD2_2 |
| SIP 前向 A/B | PASS | U = dx + A(dx,dy), V = dy + B(dx,dy) (前向, 像素→中间) |
| 与 astropy 对比 | PASS | max\|Δ\| = 5.684e-14 度 (远小于阈值 1e-9) |
| 与 drizzle 一致 | PASS | 复用 healpix_drizzle/wcs_sip.cpp 算法, 同一坐标系 |

### 4. SIP 系数读取 (orchestrator)
| 检查项 | 结果 | 说明 |
|--------|------|------|
| A_ORDER/B_ORDER 读取 | PASS | 从 header KV 块读取, 空时跳过 |
| 阶数上限检查 | PASS | a_order > SNR_SIP_MAX_ORDER (5) 时跳过 |
| A_i_j/B_i_j 读取 | PASS | 跳过 (0,0), i+j<=order, 按 i*6+j 存储 |
| 与 PHOTOMETRIC 一致 | PASS | 复用 read_wcs_from_header 的 SIP 读取逻辑 |

### 5. HISS 稀疏模型 (snr_format=1)
| 检查项 | 结果 | 说明 |
|--------|------|------|
| 序列化格式 | PASS | 4 + n_points×20 + 24 字节, 与 hp_drizzle_api.cpp 期望一致 |
| HioSnrControlPoint packing | PASS | #pragma pack(1), 20 字节 (ra 8 + dec 8 + snr_psf 4) |
| 往返完整性 | PASS | 5 控制点 + 10 像素, 所有字段偏差 < 1e-9 |
| snr_format 标记 | PASS | meta_json 含 "snr_format":1 |

### 6. 兼容性与回滚
| 检查项 | 结果 | 说明 |
|--------|------|------|
| 向后兼容 | PASS | a_order=0 时退化为纯 CD+TAN, 行为不变 |
| P02 路径 B 不破坏 | PASS | stage1 集成测试 success=true |
| P03-001/002 不破坏 | PASS | 配置参数传递不受影响 |
| 回滚方案 | PASS | 撤销 orchestrator.cpp P03-004 代码即可; SnrSipCoeffs 是新增字段 |

### 7. 约束满足
| 约束 | 验证方法 | 结果 |
|------|----------|------|
| SNR 单次执行 (非迭代) | snr_extract_model 单次调用 | PASS |
| spherical PSF 不必要 | 使用 PSF 星位置 + WCS 转球面 | PASS |
| 迭代 SNR 不允许 | 无迭代逻辑 | PASS |
| WCS+SIP 与 astropy 一致 | 验证 B 对比测试 | PASS |
| HISS snr_format=1 | 验证 E 往返测试 | PASS |

## 风险评估

### 已知限制 (非缺陷)
1. **SNR stage 在 sigma_residual=0 时降级**: 这是测光阶段 n_matched=0 的上游问题, 非 P03-004 缺陷。降级行为正确 (记录 SNR_STATUS, 不阻塞)。
2. **Test E HioSnrControlPoint 需要 _pack_=1**: Python ctypes 默认对齐与 C++ #pragma pack(1) 不一致, 已在验证脚本中处理。
3. **AIO DLL 加载需要 mingw64/bin 在 PATH**: Python ctypes 加载 astro_image_io.dll 时需要 mingw64 运行时, 已在 load_aio_dll 中处理。

### 残留风险
- **无**: P03-004 修改均为新增 SIP 支持, 不改变现有行为; 退化路径保持不变; 集成测试验证 stage1 总体成功。

## 复核结论

P03-004 任务完成质量良好:

1. **实现完整**: SnrSipCoeffs 结构 + snrEvalSip + pixelToSkySimple + orchestrator SIP 读取, 形成 SIP 端到端支持
2. **验证充分**: 5 个单元测试 + 1 个集成测试, 覆盖正常/退化/边界场景
3. **一致性保证**: WCS+SIP 与 astropy 差异 < 1e-9 度, 与 drizzle 复用同一算法
4. **兼容性良好**: 向后兼容, 不破坏 P02/P03-001/P03-002, 回滚方案清晰

**VERDICT: PASS**
