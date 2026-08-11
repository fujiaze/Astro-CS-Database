# lib/phase2/tools/config_smoke.py — G7 Stage2 config/schema smoke
#
# 校验：
#   1. official template 通过 production schema（jsonschema）；
#   2. template parse 通过（astrocs-stage2 配置解析成功，随后因输入路径
#      不存在而 coverage 失败——证明 parse 阶段无异常退出）；
#   3. 每种 rejection enum parse smoke；
#   4. 非法 smoothing/类型返回清晰错误而非异常退出。
import json
import os
import subprocess
import sys

ROOT = r"F:\Astro dev\Astro CS Normalization Database"
SCHEMA = os.path.join(ROOT, "工程控制", "schemas", "stage2.schema.json")
TEMPLATE = os.path.join(ROOT, "工程控制", "configs", "stage2.template.json")
EXE = os.path.join(ROOT, "lib", "phase2", "build", "astrocs-stage2.exe")


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def validate_schema():
    try:
        import jsonschema
    except ImportError:
        print("[config] jsonschema 不可用，跳过 schema 校验（结构手动核对）")
        return True
    schema = load(SCHEMA)
    template = load(TEMPLATE)
    jsonschema.validate(template, schema)
    print("[config] template 通过 production schema")
    return True


def run_parse(cfg_text, expect_parse_ok=True):
    tmp = os.path.join(ROOT, "run", "temp", "config_smoke.json")
    with open(tmp, "w", encoding="utf-8") as f:
        f.write(cfg_text)
    r = subprocess.run([EXE, tmp], capture_output=True, text=True,
                       encoding="utf-8", errors="replace", timeout=60)
    out = (r.stdout or "") + (r.stderr or "")
    parse_failed = "config error:" in out or "config parse 失败" in out
    if expect_parse_ok:
        assert not parse_failed, "parse 不应失败:\n" + out[:800]
    else:
        assert parse_failed, "非法配置应报清晰错误:\n" + out[:800]
    return out


def main():
    ok = True
    ok &= validate_schema()
    template = load(TEMPLATE)
    template["output"]["hips"] = "run/temp/config_smoke_out.hips"
    # 2. template parse：应 parse 成功，然后因输入路径不存在而 coverage 失败
    out = run_parse(json.dumps(template), expect_parse_ok=True)
    assert "coverage error" in out or "coverage" in out, "应进入 coverage 阶段"
    print("[config] template parse PASS（进入 coverage 阶段）")
    # 3. 每种 rejection enum parse smoke
    methods = ["none", "sigma", "winsorized_sigma", "averaged_sigma",
               "linear_fit", "generalized_esd", "rcr"]
    for m in methods:
        cfg = json.loads(json.dumps(template))
        cfg["integration"]["rejection"]["method"] = m
        run_parse(json.dumps(cfg), expect_parse_ok=True)
    print(f"[config] rejection enum parse smoke PASS（{len(methods)} 种）")
    # 4. 非法输入 → 清晰错误
    bad_smooth = json.loads(json.dumps(template))
    bad_smooth["model"]["smoothing"] = "banana"
    run_parse(json.dumps(bad_smooth), expect_parse_ok=False)
    bad_type = json.loads(json.dumps(template))
    bad_type["integration"]["precision"] = "int8"
    run_parse(json.dumps(bad_type), expect_parse_ok=False)
    bad_wm = json.loads(json.dumps(template))
    bad_wm["integration"]["weight_mode"] = "snr2"
    run_parse(json.dumps(bad_wm), expect_parse_ok=False)
    print("[config] 非法输入清晰错误 PASS")
    print("CONFIG_SMOKE=" + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
