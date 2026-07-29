# 测试报告

- Task：P11-001 冻结内部/图像/FITS/WCS坐标约定
- Date：2026-07-27
- 环境：Windows + Python 3.11 + PowerShell 7

## 测试矩阵

| 测试维度 | 测试点数 | 通过 | 失败 | 跳过 |
|---------|---------|------|------|------|
| contract | 7 | 7 | 0 | 0 |
| unit | 6 | 6 | 0 | 0 |
| consistency | 4 | 4 | 0 | 0 |
| forbidden_shortcut | 1 | 1 | 0 | 0 |
| deliverable | 1 | 1 | 0 | 0 |
| **总计** | **19** | **19** | **0** | **0** |

## 测试详情

### Contract 测试（7 项 — 验证冻结约定与代码一致）

| # | 测试名 | 验证内容 | 结果 |
|---|--------|---------|------|
| 1 | S2_center_no_plus_half | 内部中心点 cx=img_w/2.0, cy=img_h/2.0（无 +0.5） | PASS |
| 2 | S2_y_flip | U.y = -(det_y - cy)（Y 反转 down→up） | PASS |
| 3 | S3_crpix_1based | CRPIX = width/2.0 + 0.5（1-based FITS） | PASS |
| 4 | S3_cd_no_cosdec | CD = TRANS/3600（无 1/cos(Dec) 因子） | PASS |
| 5 | S3_y_up_to_down | CD.cd12/cd22 取反（Y-up→Y-down 转换） | PASS |
| 6 | S3_sip_sign | SIP A *= (-1)^j, B *= -(-1)^j | PASS |
| 7 | S3_crval_crpix_unchanged | Y-flip 后 CRVAL/CRPIX 不变 | PASS |

### Unit 测试（6 项 — 验证模块内部约定）

| # | 测试名 | 验证内容 | 结果 |
|---|--------|---------|------|
| 8 | aio_shape_h_w | astro_image_io shape=(height,width) | PASS |
| 9 | aio_crpix_passthrough | CRPIX 原样读写（无 ±1 转换） | PASS |
| 10 | aio_has_wcs | has_wcs 判定：CTYPE 非空 + CD |val|>1e-15 | PASS |
| 11 | photometric_crpix_pattern | Photometric: dx = x - (crpix1 - 1.0) | PASS |
| 12 | photometric_no_explicit_cosdec | Photometric 无显式 cos(Dec) 乘法 | PASS |
| 13 | drizzle_crpix_pattern | Drizzle: dx = x - (crpix[0] - 1.0) + 1-based 注释 | PASS |

### Consistency 测试（4 项 — 验证模块间一致性）

| # | 测试名 | 验证内容 | 结果 |
|---|--------|---------|------|
| 14 | iface_itertrans_u_comment | ipv_itertrans.h U 坐标定义注释存在 | PASS |
| 15 | iface_sip_cosdec_comment | ipv_sip.h cos(dec) 公式注释存在 | PASS |
| 16 | iface_api_crpix_comment | ipv_api.h CRPIX 1-based 注释存在 | PASS |
| 17 | iface_wcs_transform_block | wcs_transform.h 坐标约定注释块存在 | PASS |

### Forbidden Shortcut 测试（1 项 — 禁止捷径）

| # | 测试名 | 验证内容 | 结果 |
|---|--------|---------|------|
| 18 | forbidden_no_code_modification | git diff 确认无代码文件修改 | PASS |

### Deliverable 测试（1 项 — 交付物存在）

| # | 测试名 | 验证内容 | 结果 |
|---|--------|---------|------|
| 19 | deliverable_convention_doc | COORDINATE_CONVENTION.md 存在 (>5KB) | PASS |

## 测试输出原文

```
======================================================================
P11-001 坐标约定冻结验证
======================================================================
[PASS] S2_center_no_plus_half — cx=img_w/2.0 cy=img_h/2.0 (无 +0.5)
[PASS] S2_y_flip — U.y = -(det_y - cy)
[PASS] S3_crpix_1based — CRPIX = width/2.0 + 0.5
[PASS] S3_cd_no_cosdec — CD = TRANS/3600 (无 cos(Dec) 因子)
[PASS] S3_y_up_to_down — CD.cd12/cd22 取反
[PASS] S3_sip_sign — A *= (-1)^j, B *= -(-1)^j
[PASS] S3_crval_crpix_unchanged — CRVAL/CRPIX 不被取反
[PASS] aio_shape_h_w — shape=(height,width)
[PASS] aio_crpix_passthrough — CRPIX 原样读写 (无 ±1)
[PASS] aio_has_wcs — CTYPE 非空 + CD |val|>1e-15
[PASS] photometric_crpix_pattern — dx = x - (crpix1 - 1.0)
[PASS] photometric_no_explicit_cosdec — 无显式 cos(Dec) 乘法
[PASS] drizzle_crpix_pattern — dx = x - (crpix[0] - 1.0), CRPIX 1-based 注释存在
[PASS] iface_itertrans_u_comment — U 坐标定义注释存在
[PASS] iface_sip_cosdec_comment — cos(dec) 公式注释存在
[PASS] iface_api_crpix_comment — CRPIX 1-based 注释存在
[PASS] iface_wcs_transform_block — 坐标约定注释块存在
[PASS] forbidden_no_code_modification — 无代码修改 (modified=[])
[PASS] deliverable_convention_doc — COORDINATE_CONVENTION.md 存在 (13229 bytes)
======================================================================
总计: 19  通过: 19  失败: 0
======================================================================
```

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 |
|------|------|--------|
| `python engineering_v1.2/evidence/P11-001/scripts/verify_convention.py` | 60s | 0 |

## 结论

19/19 测试 PASS。覆盖 contract（7）+ unit（6）+ consistency（4）+ forbidden shortcut（1）+ deliverable（1）五个维度。所有冻结约定与代码实际状态一致，无代码修改，禁止捷径检查通过。
