> **ARCHIVED_NON_NORMATIVE** — GOV-002 归档历史技术文档，不再作为当前权威。
> 替代文档：docs/science/PHASE2_UPM.md、docs/contracts/API-001.md、docs/algorithms/PHASE2_SAMPLER.md

# Phase2 工程文档 (V19)

## 权重模型 (SNR_REDESIGN_CONTRACT §11-13)

### UPM 控制权重 (upm.robust_control_weight.v1 → V19)

```text
w_UPM = quality × support^p × ivar_eff
ivar_eff = obs->ivar (帧 ivar 产品控制 leaf 值, >0 优先)
         = 1/uncertainty² (回退; Var(control_estimator))
legacy snr²/(1+snr²): 仅 use_ivar_weight=0 时用于 ablation (SNR-015)
```

### Integration 权重 (stack.ivar.v1 默认)

```text
weight_mode = 2 (默认 ivar)
  逐像素权重 = 帧 ivar 产品像素值 (Drizzle 传播方差倒数)
  support 只作 validity/coverage (禁止与 ivar 双重计数)
  产品缺失 → support (几何可靠性) 回退, 如实计数 ivar_product_missing
weight_mode = 1  equal
weight_mode = 0  support_x_snr2 (legacy, 仅 ablation/诊断)
```

### ACR kernel

```text
mode 2: stack_w = support × ivar
mode 0: stack_w = support × snr² (legacy)
```

## 管线

```text
DISCOVER → VALIDATE → COVERAGE_UNION → CONTROL_SAMPLE → UPM_FIT →
UPM_PERSIST → BLOCK_PLAN → BLOCK_CALIBRATE → REJECT_INTEGRATE →
HIPS_WRITE → HIPS_VERIFY
```

## 验收 (V19 相关)

```text
UPM-009 no raw-star-SNR weight   PASS (science path 无 snr² 因子)
UPM-010 control uncertainty      PASS (ivar 或 1/unc²)
INT-002 inverse-variance optimum PASS (SNR-009 分析 + gate)
INT-004 rescale+propagated var   PASS (DRZ-014 + UPM ablation)
INT-008 signal-independent       PASS (SNR-010)
gate 65/65 → 74/74 (V19 增加 UPM ivar 权重门)
```
