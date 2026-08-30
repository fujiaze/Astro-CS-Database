# phase1/wcs — WCS TAN (L2 模块 README)

- 合同: `P1-004` / SCI-WCS-001 (docs/contracts/INDEX.yaml)
- Header: `lib/phase1/wcs/wcs_tan.h`
- Source: `lib/phase1/wcs/wcs_tan.cpp`
- Test: `tests/unit/p1_wcs_phot_test.cpp` (roundtrip <1e-6 deg)

## 职责
TAN 投影 pixel↔sky (ICRS deg)。pixel→sky→pixel roundtrip <1e-6。
合同公式见 SCI-WCS-001; 本模块只实现投影几何, 不涉畸变。
