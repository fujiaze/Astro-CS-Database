# Rejection Science

## 目的

识别并排除每像素的异常候选（卫星线、宇宙线、云、坏帧），使积分稳健。

## 科学定义

逐像素候选栈（candidate stack）经规划方法过滤：

```text
None / Sigma / Winsorized / AveragedSigma / LinearFit / GeneralizedESD / RCR
```

生产默认：auto + profile=wbpp_2_9_1/astrocs_adaptive
（n<6 percentile、6-15 winsorized、>15 linear_fit）。

## 关键语义（V15 冻结 RJ-001..008）

- eligibility 分层：invalid_finite / invalid_support / 显式 reason；
- INVALID_* 状态 → 调用方 hard fail（非可继续集合）；
- UNDERDETERMINED = 样本不足（≤min_samples），不做猜测积分；
- large_scale 结构生长仅对扩展结构，compact cosmic 不生长（`rejection.cpp:1501-1592` trail 生长分支）。

## 变量/单位

- 信号：同积分信号单位；支持度 support ∈ [0,1]。

## 假设

- 每像素候选独立；噪声近似对称（稳健中位数尺度）。

## 有效域

- n≥min_samples；n≤2 生产运行如实 UNDERDETERMINED。

## 不保证

- 不保证分离真实瞬变信号（同轨道卫星）与剔除目标的语义区分。

## 失效条件

- NaN/Inf 权重或 support → INVALID_INPUT；
- 全拒 → ALL_REJECTED；无候选 → NO_CANDIDATES。

## 数值精度

FP64；ESD/RCR 参照 NIST 独立实现验证。

## 参考文献

Maples et al. (2018) RCR；Rosner (1983) ESD；WBPP 2.9.1 源码策略。

## ID

ALG-REJ-001..008（RJ-001..008）；SCI-REJ-*（S2 补注册）。
