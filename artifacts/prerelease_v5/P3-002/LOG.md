# P3-002 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS P3-002 行(实现 FITS-WCS output descriptor、pixel-center world transform 和反变换;验收=独立 WCS roundtrip;RA wrap/pole/rotation;CRPIX/CD keywords 正确); PHASE3_API_V1 §2/§4(仅 TAN;|dec|≥5°;跨半球拒)。

## 动作
1. lib/phase3_session/p3_wcs.{h,cpp}: P3WcsDescriptor(crpix FITS 1-based pixel-center=(W+1)/2/crval ICRS/CD[2][2]/尺寸/projection)+p3_wcs_make(守卫: |dec|≥5°、scale>0、W/H∈[1,20000]、parity∈{east_left,east_right}、east_left→CD1_1<0;PA 旋转入 CD 矩阵;**四角同半球守卫**)+p3_wcs_pix2world(CD·δ→(ξ,η)→Calabretta 球面三角逆: δ=asin(...), Δα=atan2(−cosθ sinφ, cosδ0 sinθ−sinδ0 cosθ cosφ);RA 归一 [0,360))+p3_wcs_world2pix(gnomonic 正解+CD⁻¹ 线性解;denom≤0→半球拒;|dec|≥5° 守卫)+p3_wcs_fits_keywords(CTYPE/CRPIX/CRVAL/CD/CUNIT)。
2. 探针 tests/backend/p3_wcs_main.cpp(make/p2w/w2p/kw)。
3. tests/backend/test_p3_wcs.py 6 测试, 其中参考实现=**独立向量法**(切平面基向量, 与 C++ 球面三角法不同推导路径): 7×7 网格×3 姿态 roundtrip(≤1e-7 px)+与独立参考一致(≤1e-8 deg)/RA wrap(中心 359.9 east_right→跨 0° 归一小正值+roundtrip)/极点守卫(dec=±88 拒,84 合法,反变换 89.5 拒)/半球拒(scale 45° 大图拒,小图过)/CRPIX=(W+1)/2 精确+CD 符号与旋转项(30°)关键字逐值断言/W-H 越界与 scale≤0 拒。
4. 过程修复(实现期): Δα 公式两处符号错(首版 arg(−ξ,η) 途径+归一在弧度域)——经独立向量法数值真值(牛顿法求逆)逐点定位: 正确式 Δα=atan2(−cosθ sinφ, cosδ0 sinθ−sinδ0 cosθ cosφ); 归一移至角度域。测试参考同步同式(向量法仍独立于 C++ 三角法)。

## 验证
- 全量回归 unittest **176/176 OK**(新增 6)。
- 数值真值交叉验证: C++ 逆变换 vs 向量法+牛顿数值解 1e-12 一致(210.060283907105)。

## 限制与遗留
- 仅 TAN(合同冻结); SIP/TPV 畸变不在 V5 范围。
- 关键字目前以文本行输出; CODE-P3 写 FITS 头时按 80 字节卡格式化落盘(原子写由 ARCH-P3 §4)。

## 产物
lib/phase3_session/p3_wcs.{h,cpp}; tests/backend/p3_wcs_main.cpp; tests/backend/test_p3_wcs.py; 本日志。

## PASS 判定
WCS 描述符/正反变换实现且经独立推导路径参考(向量法)逐点一致; roundtrip/RA wrap/pole/rotation/CRPIX+CD keywords 全部机器断言过; 显式拒绝清单(极点/半球/越界)落锤。P3-002 = PASS。
