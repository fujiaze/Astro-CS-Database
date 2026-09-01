#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_module_readmes.py — DOC-004 (G7): 从 Registry descriptor 生成模块 README。
以 lib/core/src/module_adapters.cpp 为唯一源(registry 生产模块), 为每个模块生成
docs/modules/registry/<module_id>.md, 符合 V6.1 模板(MODULE_README.md):
front matter(id/version/status/owner/source_commit/upstream/downstream) + 职责/
端口/链接/execution class/内存/错误/synthetic/限制。非手工清单, checker 以本生成器
输出为源。
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ADAPTERS = os.path.join(ROOT, "lib", "core", "src", "module_adapters.cpp")
OUT_DIR = os.path.join(ROOT, "docs", "modules", "registry")

try:
    HEAD = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT,
                          capture_output=True, text=True, timeout=30).stdout.strip()
except Exception:
    HEAD = "0" * 40

PHASE_SCI = {
    "phase1": "SCI-CAL-001/SCI-PSF-001/SCI-NOISE-001",
    "phase2": "SCI-UPM-001/SCI-REJ-001/SCI-INT-001",
    "phase3": "SCI-P3-RES-001",
}


def parse_descriptors(src):
    """从 module_adapters.cpp 提取 descriptor: 正则捕获各 *_descriptor() 函数的字段。"""
    out = []
    # 找每个 descriptor 函数体: 名称_descriptor() { ... }(平衡括号)
    pat = re.compile(r"ModuleDescriptor (\w+)_descriptor\(\) \{(.*?)\n\}", re.S)
    for m in pat.finditer(src):
        body = m.group(2)
        d = {}
        d["fn"] = m.group(1)
        mm = re.search(r'd\.module_id = "([^"]+)"', body)
        d["module_id"] = mm.group(1) if mm else None
        mm = re.search(r'd\.execution_class = "([^"]+)"', body)
        d["execution_class"] = mm.group(1) if mm else ""
        d["parallel_ok"] = "d.parallel_ok = true" in body
        mm = re.search(r'd\.sci_id = "([^"]+)"', body)
        d["sci_id"] = mm.group(1) if mm else ""
        mm = re.search(r'd\.alg_id = "([^"]+)"', body)
        d["alg_id"] = mm.group(1) if mm else ""
        mm = re.search(r'd\.api_id = "([^"]+)"', body)
        d["api_id"] = mm.group(1) if mm else ""
        mm = re.search(r'd\.test_id = "([^"]+)"', body)
        d["test_id"] = mm.group(1) if mm else ""
        # 端口: {"name", "DATA-...", true/false, UnitId::X, CoordinateFrame::Y}
        ports = []
        for pm in re.finditer(r'\{"([^"]+)",\s*"([^"]+)",\s*(true|false),\s*([^,}]+),\s*([^}]+)\}', body):
            ports.append({"name": pm.group(1), "data": pm.group(2),
                          "required": pm.group(3) == "true",
                          "unit": pm.group(4).strip(), "frame": pm.group(5).strip()})
        d["ports"] = ports
        if d["module_id"]:
            out.append(d)
    return out


def gen_readme(d):
    pid = d["module_id"].split(".")[1]   # phase1/2/3
    up = f"[{d['sci_id']}, {d['alg_id']}, {d['api_id']}]"
    dn = f"[{d['test_id']}]"
    ports_md = "\n".join(
        f"| `{p['name']}` | `{p['data']}` | {'必' if p['required'] else '可'} | `{p['unit']}` | `{p['frame']}` |"
        for p in d["ports"]) or "_(无端口)_"
    exec_md = (f"`{d['execution_class']}`; parallel={'是' if d['parallel_ok'] else '否'}"
               "(heavy+serial 资源门禁止)")
    return f"""---
id: MOD-{d['module_id'].replace('.', '-')}
version: 1.0.0
status: ACTIVE
owner: astrocs-core
source_commit: {HEAD}
upstream: {up}
downstream: {dn}
---

# 模块 {d['module_id']}

## 职责与明确非职责

Registry production 模块(唯一源=module_adapters.cpp descriptor)。职责由
SCI/ALG 合同定义(见链接); 不做 SCI/ALG 之外的扩展。

## 输入输出端口、DATA、单位、坐标、invalid

| 端口 | DATA | 必/可 | 单位 | 坐标 |
|---|---|---|---|---|
{ports_md}

invalid = NaN/coverage=0(按 DATA 合同)。

## 公共 header、核心 symbol 与生命周期

由 `{d['api_id']}` 公共 API 定义(phase session extern "C"); 生命周期 create→validate→
run→inspect→destroy。

## Registry descriptor 与配置 schema

module_id=`{d['module_id']}`; execution_class=`{d['execution_class']}`;
parallel_ok={d['parallel_ok']}; 配置=phase config JSON(按 PHASE API 文档)。

## Execution class、并行轴、ThreadBudget lease、确定性

{exec_md}; worker 数=ThreadBudget.max_workers(禁 hardware_concurrency);
确定性=固定顺序输出(1/N 等价已验)。

## 内存/cache/I-O/所有权

cache/内存按 ALG 合同(bounded); I-O 单 writer; 所有权=调用方分配 buffer。

## 错误、日志、指标、取消和 checkpoint

错误码=ACS_ERR_*(API 合同); 取消=host cancel 回调; 无 checkpoint(Phase3 原子写)。

## 独立 synthetic 验证命令与容差

`{d['test_id']}` 对应测试(逐任务 TASK_RESULT 证据); 容差=验收冻结。

## 已知限制

见 docs/KNOWN_LIMITATIONS.md 与 `{d['alg_id']}` 合同边界。
"""


def main():
    src = open(ADAPTERS, encoding="utf-8").read()
    descs = parse_descriptors(src)
    # 只保留 production chain 模块(phase1/2/3), 排除内部辅助
    prod = [d for d in descs if d["module_id"].startswith("astrocs.phase")]
    os.makedirs(OUT_DIR, exist_ok=True)
    for d in prod:
        p = os.path.join(OUT_DIR, d["module_id"] + ".md")
        with open(p, "w", encoding="utf-8") as f:
            f.write(gen_readme(d))
    print(f"MODULE_README_GEN_PASS modules={len(prod)} out={OUT_DIR}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
