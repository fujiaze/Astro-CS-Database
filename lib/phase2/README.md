# AstroCS Phase2 — UPM + 分块排异叠加 → HiPS 马赛克

控制包：`AstroCS_Phase2_Implementation_Control_Package_V1`
（SHA `34A532A2451C8746BEF7B5DA05C3C4C7D15201D66A9D5F6AB5F8F291BE2EB308`）。

## 构建

```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cd lib\phase2\build
cmake .. -G Ninja
ninja astrocs-stage2 phase2_synthetic_gate
```

`astrocs-stage2.exe` 依赖 `lib\astro_image_io\astro_image_io.dll`（运行 PATH 加入）。

## 运行

```powershell
astrocs-stage2 <stage2.json>
```

只允许一个 JSON 配置路径参数。示例配置见
`run/phase2/stage2_t4_overlap.json`（真实重叠验证）与
`run/phase2/stage2_full.json`（完整三片）。

## 合成 Gate

```powershell
.\phase2_synthetic_gate.exe --gtest_brief=1
```

18/18 PASS（S0/S1/S2/R1/R2/sparse=dense/block/integrate/ACR/robust +
coverage/sampler/upm roundtrip/linear-fit/RCR）。

## 目录

- `include/astro/phase2/`：冻结公共接口（upm/coverage/sampler/rejection/
  block/integrate/acr_kernels）
- `src/`：CPU reference 实现
- `tools/stage2.cpp`：正式入口
- `tests/synthetic_gate.cpp`：合成 Gate
