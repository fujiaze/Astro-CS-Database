# Module: plate_solve (ipv)

> P1-WCS-DOC 修订（2026-09-07）：本页为模块导览旧页；冻结合同落位
> lib/plate_solve/（README.md r1 + module.yaml + memory.md 三件套，
> CONTRACT_READY，entrypoint=MISSING）；registry 页
> docs/modules/registry/astrocs.phase1.wcs-platesolve.md 已同步修订。

## 职责

星表匹配 + TAN/SIP plate solve → WCS（含 SIP A/B/AP/BP 与质量指标
rms_px/rms_arcsec/n_pairs/trans_order）。

## 非职责

不重采样图像；不做星点检测（消费 star_measurements/star_det 权威块，
禁重检测）；不做星表缓存管理（gaia 句柄注入）；不做流量定标。

## Public API

API-WCS-001（docs/contracts/PUBLIC_API.md；ipv_api.h 唯一权威签名头，
12 导出，生产入口 ipv_solve_from_detections_v1）。

## Data contract

DATA-P1-WCS（docs/contracts/DATA_SEMANTICS.md §18；detections [n,6]
+0.5 契约 → IpvWcsResult POD + inlier [n,9]；编排写回 CTYPE/CRVAL/
CRPIX/CD/RADESYS=ICRS/EQUINOX=2000/SIP）。

## Ownership

求解器状态 RAII（ipv_solve_create/destroy）；IpvWcsResult 调用方分配
POD，无堆所有权转移。

## Thread safety

句柄级互斥使用；OpenMP 仅三角形投票/选星（整数归并，bitwise 与线程数
无关）；ThreadLease 未接线（DISP-WCS-005）。

## Errors

几何退化/星不足 → ret=0/success=0（error_msg 载因）→ 编排
PLATESOLVE_FAILED；BLOCK_MISSING（必需块缺失）；CD 退化坍缩禁止冒充解
（DISP-WCS-001 失败-置信度语义）。

## Science IDs

SCI-WCS-001（docs/science/ASTROMETRY.md，FROZEN）；ALG-WCS-001
（docs/algorithms/PLATESOLVE.md §11 逐符号锚 + DISP-WCS-001..006 +
TEST-WCS-DESIGN-001 冻结容差）。

## Tests

TEST-WCS-DESIGN-001（ALG §11.4：F1 合成线性场/F2 astropy SIP oracle/
F3 CRPIX 不变量/F4 失败语义负例/F5 bitwise 确定性/F6 WcsTan roundtrip
<1e-6 deg + 独立前向交叉 ≤1e-9 deg（B2-A1））；可执行 TEST-P1-WCS-001 由
P1-WCS-TEST 落地。

## Source files

lib/plate_solve/cpp/ipv/（ipv_entry.cpp 649 行 12 导出 + 内核 13821 行；
唯一权威签名头 include/ipv_api.h）。
