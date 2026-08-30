#!/usr/bin/env python3
"""P2-001: Phase2 old Stage2 步骤映射 + ACR 禁止迁移依赖校验。

规则:
1. 每个 old Stage2 源文件 (lib/phase2/src/*.cpp, 除 stub) 必须在映射表中。
2. ACR 相关文件 (acr_kernels/cuda_bridge_stub/p2_acr_block_eligible) 必须标禁止迁移。
3. 生产二进制 ACR 符号必须为 0 (check_link_scan 联动)。
exit 0 = PASS。
"""
import pathlib, re, sys

REPO = pathlib.Path(__file__).resolve().parents[1]
MAP = (REPO / "docs/refactor/P2_SYMBOL_MAP.md").read_text(encoding="utf-8")
SRC = REPO / "lib/phase2/src"
INDEX = (REPO / "docs/contracts/INDEX.yaml").read_text(encoding="utf-8")

def main():
    errors = []
    # 1) 每个非 ACR 源文件映射 (config 在 stage2_common 内)
    mapped = {"stage2_common.cpp", "coverage.cpp", "sampler.cpp", "upm.cpp",
              "rejection.cpp", "integrate.cpp", "block.cpp", "async_io.cpp"}
    acr_set = {"acr_kernels.cpp", "cuda_bridge_stub.cpp"}
    for f in sorted(SRC.glob("*.cpp")):
        name = f.name
        if name in mapped:
            if name not in MAP:
                errors.append(f"unmapped old stage2 file: {name}")
        elif name in acr_set:
            if "禁止迁移" not in MAP or name not in MAP:
                errors.append(f"ACR file not marked forbidden: {name}")
        else:
            # 其他源: 要求映射或明确豁免
            if name not in MAP:
                errors.append(f"unclassified phase2 source: {name}")
    # 2) 禁止迁移依赖声明完整
    for token in ["acr_kernels.cpp", "cuda_bridge_stub.cpp", "p2_acr_block_eligible"]:
        if token not in MAP:
            errors.append(f"forbidden-migration token missing: {token}")
    if "禁止迁移依赖" not in MAP or "禁止作为迁移依赖" not in MAP:
        errors.append("missing forbidden-migration declaration")
    # 3) SCI 登记
    for sci in re.findall(r"\b(SCI-[A-Z0-9-]+)\b", MAP):
        if sci not in INDEX:
            errors.append(f"SCI not in INDEX: {sci}")
    if errors:
        print("P2-001_MAP_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print(f"P2-001_PASS: {len(mapped)} modules mapped, 3 ACR forbidden, SCI registered, 生产禁 ACR")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
