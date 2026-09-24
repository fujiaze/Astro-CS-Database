#!/usr/bin/env python3
"""CLI-RUN-PRESET: 运行面（preset→IR→Runtime）+ Artifact 哈希链 校验（§6.2 新命令树）。

历史: 本检查器原判据是旧字面量 '{"phase1 run"}' 等（旧 phase 命令面）。CLI-001 把用户
命令树切换为 ASTROCS_DESIGN §6.2 的唯一七行树后旧判据恒红；本文件按 TEST-CLI-SYNC 把判据
同步到新树，**判据语义保持不变**（逐命令单 phase 调度 / artifact 进 run manifest /
sha256 mismatch → INTEGRITY(8) / --events-jsonl stdout 纪律），改为实测运行新命令 +
预设旗标互斥/组合语义。

判据（能红能绿；负例自证见 --self-test）:
  [A] 命令面 = §6.2 唯一命令树: normalize/mosaic/export 各自可运行(--json)、可生成模板
      (--template/-o)、可 --help；旧 phase1/2/3 命令面（run|validate|plan|inspect 及裸
      phaseN）不再登记。
  [B] preset→IR→Runtime 唯一执行路径: 三个用户命令各自经 run_with_resource_gate(ev,
      "phaseN") → run_pipeline({N}) 做**单 phase** 调度，互不串接（禁止把三阶段隐式串成
      一次运行, §1.2）。
  [C] artifact 传递: artifact 收集进 run manifest（artifacts 数组 + astrocs_run_*.json）；
      resume 时 prior artifact sha256 与磁盘不符 → INTEGRITY(=8)，绝不静默跳过。
  [D] 预设旗标互斥/组合语义（binary 存在时实测运行）:
      * '<cmd> --template [-o <path>]' rc=0，-o 落盘文档 == --template --json 文档；
      * '<cmd> --json <cfg> --template' rc=2（运行/模板互斥）；
      * '<cmd> --json'（缺值）→ 2；未知旗标 → 2；'--json <不存在>' → 3；
      * 旧命令 'phaseN run' → 2；
      * '--events-jsonl' 下 stdout 只有 JSON 事件（无日志污染），预检阻断（空输入）不得
        留下 status=complete 的 manifest。

退出 0 = PASS；1 = 违规（打印 CLI-RUN-PRESET_VIOLATION 明细）。
负例自证: python3 eng/tools/check_cli_run_preset.py --self-test
  - 命令树副本重新登记旧命令 phase1 run → 必须红；
  - commands.cpp 副本删掉 artifact 收集 → 必须红；
  - 用永远 rc=0 的桩二进制跑运行时判据 → 必须红；
  - 恢复后必须重新变绿。
"""
import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]

# §6.2 唯一命令树里三个运行命令 → 内部会话号（phase 仅内部指代）。
RUN_COMMANDS = {"normalize": 1, "mosaic": 2, "export": 3}
REQUIRED_FLAGS = ("--json", "--template", "-o", "--help")
LEGACY_TREE_TOKENS = ("phase1", "phase2", "phase3")
# 每个运行命令的最小可运行配置键（fail-closed 空输入 → 预检阻断 rc=2）
CFG_KEY = {"normalize": "input_lights", "mosaic": "hips_paths", "export": "source"}


def read(path):
    p = pathlib.Path(path)
    return p.read_text(encoding="utf-8") if p.is_file() else ""


def parse_tree_commands(text):
    """解析 command_tree.h::commands() 的 {path, public, {flags}} 三元组。"""
    m = re.search(r"commands\(\)\s*\{(.*?)\n    \};", text, re.S)
    if not m:
        return None
    out = {}
    for mm in re.finditer(r'\{\s*"([^"]+)"\s*,\s*(true|false)\s*,\s*\{([^}]*)\}', m.group(1)):
        out[mm.group(1)] = set(re.findall(r'"([^"]+)"', mm.group(3)))
    return out


def strip_line_comments(text):
    return re.sub(r"//[^\n]*", "", text)


def check_static(root=None):
    """静态判据（[A][B][C]）。root 可指向临时副本（负例用）。"""
    root = pathlib.Path(root) if root else REPO
    errors = []

    tree_text = read(root / "lib" / "infrastructure" / "cli" / "command_tree.h")
    cmds = parse_tree_commands(tree_text) if tree_text else None
    if not cmds:
        return ["命令树缺失或不可解析: lib/infrastructure/cli/command_tree.h"]

    # [A] §6.2 运行命令 + 旧命令面消失
    for cmd in RUN_COMMANDS:
        if cmd not in cmds:
            errors.append("§6.2 运行命令缺失: %s" % cmd)
            continue
        for flag in REQUIRED_FLAGS:
            if flag not in cmds[cmd]:
                errors.append("%s 缺旗标 %s（§6.2 运行/模板/帮助面）" % (cmd, flag))
    for path in cmds:
        if path.split()[0] in LEGACY_TREE_TOKENS:
            errors.append("旧 phase 命令重新登记: %s（§6.2 唯一命令树）" % path)

    # [B][C] 调度面与 artifact 哈希链（注释盲区加固: 剥 // 行注释后匹配）
    cmds_text = strip_line_comments(read(root / "lib" / "infrastructure" / "cli" / "commands.cpp"))
    if not cmds_text:
        errors.append("lib/infrastructure/cli/commands.cpp 缺失（无法核对调度面）")
    else:
        for cmd, sid in RUN_COMMANDS.items():
            token = 'run_with_resource_gate(ev, "phase%d"' % sid
            if token not in cmds_text:
                errors.append("缺 %s 的单 phase 调度 %s（preset→IR→Runtime）" % (cmd, token))
        if "run_pipeline({" not in cmds_text:
            errors.append("缺 run_pipeline 单 phase 调度 helper（preset→IR→Runtime 唯一路径）")
        if '"artifacts", artifacts' not in cmds_text:
            errors.append("缺 artifact 收集进 run manifest")
        if "astrocs_run_" not in cmds_text:
            errors.append("缺 run manifest 落盘模式 astrocs_run_")
        if "artifact sha256 mismatch" not in cmds_text:
            errors.append("缺 artifact sha256 mismatch 守卫")
        if "astrocs::INTEGRITY" not in cmds_text:
            errors.append("缺 sha256 mismatch → INTEGRITY(=8) 退出码")

    # [D] 事件模式旗标在解析器旗标面（--events-jsonl）
    if "--events-jsonl" not in read(root / "lib" / "infrastructure" / "cli" / "parser.cpp"):
        errors.append("parser 旗标面无 --events-jsonl（stdout 事件模式）")
    return errors


def _rc(binary, args, cwd, timeout=60):
    try:
        r = subprocess.run([binary, *args], capture_output=True, text=True,
                           encoding="utf-8", errors="replace", timeout=timeout, cwd=cwd)
        return r.returncode, r.stdout, r.stderr
    except (OSError, subprocess.TimeoutExpired) as e:
        return "run-error:%s" % e, "", ""


def check_runtime(binary, timeout=60):
    """[D] 预设旗标互斥/组合语义 + 运行面纪律（实测运行）。"""
    errors = []
    binary = str(binary)
    if not os.path.isfile(binary):
        return ["binary 不存在: %s" % binary]

    tmp = tempfile.mkdtemp(prefix="cli_run_preset_")
    try:
        out_dir = os.path.join(tmp, "out")
        os.makedirs(out_dir)
        cfgs = {}
        for cmd in RUN_COMMANDS:
            p = os.path.join(tmp, "%s_cfg.json" % cmd)
            with open(p, "w", encoding="utf-8") as fh:
                json.dump({"schema_version": "1", "output_dir": out_dir,
                           CFG_KEY[cmd]: []}, fh)
            cfgs[cmd] = p

        for cmd in RUN_COMMANDS:
            cfg = cfgs[cmd]
            # 组合: --template（开关）rc=0 且 stdout 恰一 JSON 文档
            rc, out, err = _rc(binary, [cmd, "--template"], tmp, timeout)
            if rc != 0:
                errors.append("预设组合: %s --template 应 rc=0 → rc=%s (%s)"
                              % (cmd, rc, err.strip()[:120]))
            else:
                try:
                    json.loads(out)
                except ValueError:
                    errors.append("预设组合: %s --template stdout 不是恰一 JSON 文档" % cmd)
            # 组合: --template -o <path> 落盘 == --template --json stdout
            tpl = os.path.join(tmp, "%s_tpl.json" % cmd)
            rc2, _out2, err2 = _rc(binary, [cmd, "--template", "-o", tpl], tmp, timeout)
            if rc2 != 0 or not os.path.isfile(tpl):
                errors.append("预设组合: %s --template -o <path> 应 rc=0 且落盘 → rc=%s (%s)"
                              % (cmd, rc2, err2.strip()[:120]))
            else:
                rc3, out3, _e3 = _rc(binary, [cmd, "--template", "--json"], tmp, timeout)
                if rc3 != 0:
                    errors.append("预设组合: %s --template --json 应 rc=0 → rc=%s" % (cmd, rc3))
                else:
                    try:
                        on_disk = json.loads(pathlib.Path(tpl).read_text(encoding="utf-8"))
                        if json.loads(out3) != on_disk:
                            errors.append("预设组合: %s -o 落盘文档 != --template --json 文档" % cmd)
                    except ValueError:
                        errors.append("预设组合: %s --template --json stdout 非 JSON" % cmd)
            # 互斥: --json <path> 与 --template 不得同时成立 → 2
            rc4, _o4, err4 = _rc(binary, [cmd, "--json", cfg, "--template"], tmp, timeout)
            if rc4 != 2:
                errors.append("预设互斥: %s --json <cfg> --template 应 rc=2 → rc=%s (%s)"
                              % (cmd, rc4, err4.strip()[:120]))
            # 参数错误 → 2（缺值 / 未知旗标）
            for args in ([cmd, "--json"], [cmd, "--json", cfg, "--bogus"]):
                rc5, _o5, _e5 = _rc(binary, args, tmp, timeout)
                if rc5 != 2:
                    errors.append("参数错误应 rc=2: acsd %s → rc=%s" % (" ".join(args), rc5))
            # 输入缺失 → 3（§6.3 码表）
            rc6, _o6, _e6 = _rc(binary, [cmd, "--json", os.path.join(tmp, "nope.json"), "-y"],
                                tmp, timeout)
            if rc6 != 3:
                errors.append("输入缺失应 rc=3: %s --json <不存在> → rc=%s" % (cmd, rc6))
            # 运行面: 空输入预检阻断 → 2；--events-jsonl stdout 只有 JSON；不落 complete
            rc7, out7, _e7 = _rc(binary, [cmd, "--json", cfg, "--events-jsonl", "-y"],
                                 tmp, timeout)
            if rc7 != 2:
                errors.append("预检阻断应 rc=2（空输入 fail-closed）: %s → rc=%s" % (cmd, rc7))
            for line in out7.splitlines():
                if line.strip() and not line.lstrip().startswith("{"):
                    errors.append("--events-jsonl stdout 夹带非 JSON 文本: %s" % line[:80])
                    break
            for fn in sorted(os.listdir(out_dir)):
                if fn.startswith("astrocs_run_"):
                    try:
                        doc = json.loads(pathlib.Path(out_dir, fn).read_text(encoding="utf-8"))
                    except ValueError:
                        continue
                    if doc.get("status") == "complete":
                        errors.append("预检阻断仍写 complete manifest: %s/%s" % (cmd, fn))

        # 旧命令面不得复活（§6.2 + §1.2）
        for args in (["phase1", "run"], ["phase2", "run"], ["phase3", "run"],
                     ["phase1"], ["run", "--phases", "1,2,3"]):
            rc8, _o8, _e8 = _rc(binary, args, tmp, timeout)
            if rc8 != 2:
                errors.append("旧命令应 rc=2: acsd %s → rc=%s" % (" ".join(args), rc8))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return errors


def _stub_binary(dirpath, script):
    p = os.path.join(dirpath, "astrocs_stub")
    pathlib.Path(p).write_text(script, encoding="utf-8")
    os.chmod(p, 0o755)
    return p


def self_test():
    """负例自证：故意破坏 → 检查器必须报红；恢复后必须变绿。"""
    failures = []
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="cli_run_preset_selftest_"))
    try:
        for rel in ("lib/infrastructure/cli", "cli"):
            dst = tmp / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(REPO / rel, dst, dirs_exist_ok=True)
        base = check_static(tmp)
        if base:
            failures.append("副本基线不为绿（自证无效）: %s" % base[:3])

        # 负例 1: 命令树重新登记旧命令 phase1 run
        tree = tmp / "lib" / "infrastructure" / "cli" / "command_tree.h"
        t = tree.read_text(encoding="utf-8")
        if '{"help",' not in t:
            failures.append("负例 1 注入点未命中（命令表格式变化）")
        else:
            tree.write_text(t.replace('{"help",',
                                      '{"phase1 run", true, {"--json"}},\n        {"help",', 1),
                            encoding="utf-8")
            if not check_static(tmp):
                failures.append("负例 1 未变红: 重新登记旧命令 phase1 run 仍 PASS")
            tree.write_text(t, encoding="utf-8")

        # 负例 2: 删掉 artifact 收集进 run manifest
        cp = tmp / "lib" / "infrastructure" / "cli" / "commands.cpp"
        c = cp.read_text(encoding="utf-8")
        if '"artifacts", artifacts' not in c:
            failures.append("负例 2 注入点未命中（artifact 收集字面量已变）")
        else:
            cp.write_text(c.replace('"artifacts", artifacts', '"artifacts_disabled", artifacts'),
                          encoding="utf-8")
            if not check_static(tmp):
                failures.append("负例 2 未变红: 删掉 artifact 收集仍 PASS")
            cp.write_text(c, encoding="utf-8")

        if check_static(tmp):
            failures.append("副本恢复后仍为红（自证不可复跑）")

        # 负例 3（运行时）: 永远 rc=0 的桩 → [D] 必须报红
        stub = _stub_binary(str(tmp), "#!/bin/sh\nexit 0\n")
        if not check_runtime(stub):
            failures.append("负例 3 未变红: 桩二进制（永远 rc=0）通过了运行时判据")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return failures


def find_binary(explicit=None):
    if explicit and os.path.isfile(explicit):
        return explicit
    env = os.environ.get("ASTROCS_CLI_BIN")
    if env and os.path.isfile(env):
        return env
    for rel in ("build/acsd", "build/cli/astrocs"):
        cand = REPO / rel
        if cand.is_file():
            return str(cand)
    return None


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--binary", default=None,
                    help="被测 acsd（默认探测 build/acsd、build/cli/astrocs 或 ASTROCS_CLI_BIN）")
    ap.add_argument("--static-only", action="store_true", help="只跑静态判据")
    ap.add_argument("--self-test", action="store_true", help="负例自证（必须能红）")
    args = ap.parse_args(argv)

    if args.self_test:
        fails = self_test()
        if fails:
            print("CLI-RUN-PRESET_VIOLATION (self-test):")
            for f in fails:
                print("  " + f)
            return 1
        print("CLI-RUN-PRESET_SELFTEST_PASS: 负例(旧命令重登记/删 artifact 收集/运行时桩)"
              "均能变红, 恢复后变绿")
        return 0

    errors = check_static()
    mode = "static"
    if not args.static_only:
        binary = find_binary(args.binary)
        if binary:
            errors += check_runtime(binary)
            mode = "static+runtime(%s)" % binary
        else:
            mode = "static (no binary found; runtime preset matrix skipped)"
    if errors:
        print("CLI-RUN-PRESET_VIOLATION:")
        for e in errors:
            print("  " + e)
        return 1
    print("CLI-RUN-PRESET_PASS: §6.2 三运行命令单 phase 调度, 预设互斥/组合语义, "
          "artifact 进 run manifest, sha256 mismatch→8, events-jsonl [%s]" % mode)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
