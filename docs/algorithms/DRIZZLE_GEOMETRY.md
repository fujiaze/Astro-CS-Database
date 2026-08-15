# Drizzle Geometry

关联：SCI-DRZ-001；模块：lib/healpix_db/healpix_drizzle。

## 输入

输入帧（像素 + WCS）+ 目标 HEALPix 网格 + pixfrac/drop。

## 输出

目标像素候选集 + 重叠面积权重。

## Preconditions

WCS 可逆；目标 order 有效。

## Postconditions

候选保守（false negative=0）；面积守恒 Σw 精确。

## Invariants

球面 S-H 重叠 vs 平面近似误差受控；边界/极区无漏。

## 复杂度

候选 O(pixels × 投影)；geometry cache 复用。

## 并行模型

OpenMP 按源帧 tile；只读 cache。

## 数值风险

极区奇点；wcs 数值发散 → 保守 reject。

## fast/reference/oracle

candidate oracle（全枚举对照，false negative=0）；overlap oracle
（逐像素面积核对）——evidence/drizzle/*.json。

## ID

ALG-DRZ-CAND-001；ALG-DRZ-OVERLAP-001..；TEST-ALG-DRZ-*。
