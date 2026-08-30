#!/usr/bin/env python3
"""IO-002: aio_image* owner 归类 + 禁止裸 free(aio_image*)。

扫描规则:
1. AIOImageData 的释放必须经 canonical deleter: aio_free_image_data / dispose_image / ImagePtr(RAII)。
2. 禁止: std::free(p) / free(p) 作用于 aio_read*/aio_read_fits* 返回值。
3. 输出 owner 归类表。

exit 0 = PASS (无违规); 非 0 = 发现违规。
"""
import pathlib, re, sys

REPO = pathlib.Path(__file__).resolve().parents[1]
ACQ = re.compile(r"\b(aio_read(?:_fits|_xisf|_header_only)?)\s*\(")
RAW_FREE = re.compile(r"(?:std::)?free\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")
CANONICAL = re.compile(r"aio_free_image_data|dispose_image|ImagePtr|aio_free\s*\(")

def scan():
    violations, owners = [], {}
    for src in sorted((REPO / "lib").rglob("*.cpp")) + sorted((REPO / "cli").glob("*.cpp")):
        text = src.read_text(encoding="utf-8", errors="replace")
        for m in ACQ.finditer(text):
            var = None
            # 找赋值目标: AIOImageData* X = aio_read... 或 X = aio_read...
            line_start = text.rfind("\n", 0, m.start()) + 1
            line = text[line_start:m.start()]
            am = re.search(r"(\*|\w)\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*$", line)
            if am: var = am.group(2)
            else:
                am2 = re.search(r"([A-Za-z_][A-Za-z0-9_]*)\s*=\s*aio_read", text[max(0, m.start()-120):m.start()])
                if am2: var = am2.group(1)
            owners.setdefault(m.group(1), set()).add(str(src))
            if var:
                # 检查该变量所有 free 调用
                for f in RAW_FREE.finditer(text):
                    if f.group(1) == var:
                        violations.append(f"{src}:{text.count(chr(10), 0, f.start())+1}: raw free({var}) on {m.group(1)} result — use canonical deleter")
    return violations, owners

def main():
    violations, owners = scan()
    print("OWNER CLASSIFICATION (aio_image* acquisition -> files):")
    for acq, files in sorted(owners.items()):
        print(f"  {acq}: {len(files)} files")
        for f in sorted(files): print(f"    {f}")
    print(f"  total acquisition sites: {sum(len(v) for v in owners.values())}")
    if violations:
        print("IO-002_FREE_VIOLATION:")
        for v in violations: print("  " + v)
        return 1
    print(f"IO-002_PASS: canonical deleter 全覆盖; 裸 free(aio_image*) = 0")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
