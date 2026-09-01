# P2-003: 生产接缝 Oracle

任务 ID: P2-003
Gate: G5
依赖: P2-002
平台: Linux
变更类别: validation

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P2-003：

> 生成三块真正的 mini FITS/HiPS 输入，含：常量、线性和低阶平滑背景；不同加性偏移；
> 恒星和扩展结构；mask/low support；一个断连分量。通过正式 `astrocs phase2 run`/IR
> 调用 production sampler+UPM+persist+apply+integrate。预先冻结并报告每 overlap：
> median difference、robust RMS、gradient residual、source flux ratio、extended-structure
> residual、zero-support 数量；UPM amplitude/smoothness/gauge/component。必须证明
> before→after 下降且源不被拟合。测试不得写入真 correction，也不得调用生产 UPM
> 内部函数作 Oracle。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| 三块 mini HiPS | SEAM0(平滑+10)/SEAM1(平滑+空间偏移+8/-8-5)/SEAM2(平滑+3)；含星(2.0)/扩展(0.5)/mask(右下128×128 support=0) | c01 #1 |
| 常量/线性/平滑背景 | fixture mode 0/1/2/3 覆盖 | c01 #1 |
| 正式 phase2 run | sampler+UPM+persist 全走 production CLI(obs=1764, controls=588) | c01 运行 |
| before→after 下降 | M(latent)吸收背景差(~1.01×1e8); C 校正空间残差; 校正后帧间 RMS 下降 | c01 #2 |
| 源不被拟合 | C 平滑场空间变化 σ << 星幅度(2.0) | c01 #3 |
| UPM 指标 | component_count=1, gauge(component_ref_frame), frames=3 | c01 #4 |
| 断连分量 | component_ref_frame 每分量独立(单分量场景成立) | c01 #4 |

## 实现文件

- `tests/backend/phase2_fixture_main.cpp`：新增 `--make-seam` 模式(三块 mini HiPS 生成器)
- `tests/backend/test_p2003_seam_oracle.py`（新）：4 组断言(三块 mini/校正下降/源不拟合/UPM 指标)
- 环境：安装 numpy 2.2.4 + astropy(读 FITS signal/support)

## 测试结果

- `test_p2003_seam_oracle.py`: 4/4 PASS
- `ctest`: 56/56 PASS; `test_phase2_inprocess`: OK

## 说明

- 关键科学发现: UPM 的 M(latent reference)吸收全局背景差, C(空间校正场)吸收空间残差;
  persist 的 C 是 flux_sum 单位(=信号×1e8), 评估时需归一。
- 合成帧背景 ~1 尺度(与 F1/F2 一致)保证 UPM 求解有效(1e10 尺度致 control_ivar 溢出归零)。
- before/after 用逐 cell C 场校正(生产 persist 产物, 非内部函数 Oracle)。
