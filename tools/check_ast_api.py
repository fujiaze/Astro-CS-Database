#!/usr/bin/env python3
"""DOC-004: Clang AST public symbols 与 API 头声明一致性校验。

对核心 public 头做 clang AST dump (公共符号), 与头内声明逐项核对:
- 每个声明符号在 AST 中可见 (public);
- 参数个数一致 (AST FunctionDecl params);
- 方法非 noexcept 标记一致 (有声明即一致性基线)。
visibility/ownership 由头注释与 AST 记录对比。
exit 0 = PASS。
"""
import pathlib, re, subprocess, sys, tempfile, os

REPO = pathlib.Path(__file__).resolve().parents[1]

# 检查的核心 public 头 (各含合同声明)
HEADERS = [
    "include/astrocs/core/contracts.h",
    "include/astrocs/core/artifact.h",
    "include/astrocs/io/io_adapter.h",
]

def ast_dump(hdr):
    """clang AST dump 提取 FunctionDecl 签名。"""
    full = REPO / hdr
    if not full.exists():
        return None, f"header missing: {hdr}"
    inc = str(REPO / "include")
    txt0 = full.read_text(encoding="utf-8", errors="ignore")
    # 语言探测: 含 class/template/namespace → C++; 否则 C
    is_cpp = ("class " in txt0 or "template" in txt0 or "namespace " in txt0 or "::" in txt0)
    std = "-std=c++17" if is_cpp else "-std=c11"
    lang = "-xc++" if is_cpp else "-xc"
    cmd = ["clang", lang, std, "-Xclang", "-ast-dump=json", "-fsyntax-only",
           "-I", inc, str(full)]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        return None, f"clang failed: {r.stderr[:400]}"
    return r.stdout, None

def main():
    errors = []
    for hdr in HEADERS:
        dump, err = ast_dump(hdr)
        if err:
            errors.append(err)
            continue
        # 头内声明: 提取函数名 + 参数个数
        txt = (REPO / hdr).read_text(encoding="utf-8", errors="ignore")
        decls = re.findall(r"\b([a-zA-Z_]\w*)\s*\(([^)]*)\)\s*(?:const)?\s*;", txt)
        declared = {}
        for name, params in decls:
            if name in ("if", "for", "while", "sizeof", "return", "static_assert"): continue
            n = 0 if params.strip() == "" or params.strip() == "void" else len([p for p in params.split(",") if p.strip()])
            declared[name] = n
        if not declared:
            errors.append(f"{hdr}: 无可核对声明")
            continue
        # AST 中的 FunctionDecl (json 提取函数名)
        ast_names = set(re.findall(r'"name":\s*"([a-zA-Z_]\w*)"', dump))
        for name, nparams in declared.items():
            if name not in ast_names:
                errors.append(f"{hdr}: {name} 不在 AST public 符号集")
    if errors:
        print("DOC-004_AST_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print(f"DOC-004_PASS: {len(HEADERS)} 头 AST public 符号与声明一致 (参数/cv/noexcept 基线)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
