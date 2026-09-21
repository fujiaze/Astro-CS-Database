#!/usr/bin/env python3
"""CLI-001: 唯一命令树校验（ASTROCS_DESIGN §6.1/§6.2 + §1.2）。

判据（能红能绿；负例自证见 --self-test）:
  [A] 命令树完整: lib/infrastructure/cli/command_tree.h 的命令表 == §6.2 的
      normalize/mosaic/export ×(--json|--template|--help) + help/--version/
      doctor/benchmark，且 help 文本由该表生成（无手写副本）。
  [B] 旧命令消失: 旧用户命令（phase1/2/3 及其 validate|plan|inspect|run、
      config *、modules *、selftest、test synthetic、verify*、drizzle、
      benchmark cpu|verify-profile、hardware inspect、graph、run）不再存在于
      命令表；命令表里出现任一 → 红。
  [C] 退出码稳定: 参数/命令错误恒映射 exit 2（lib/infrastructure/cli/exit_codes.h 单源），
      且 parser 只用 ParseError → ARGS 这一条参数错误出口。
  [D] 运行时 rc 矩阵（binary 存在时）: 新命令 rc=0、旧命令 rc=2 实测。

退出 0 = PASS；1 = 违规（打印 CLI-001_LAYOUT_VIOLATION 明细）。

负例（必须能红）: python3 eng/tools/check_cli_command_layer.py --self-test
  - 临时把旧命令 phase1 重新登记进命令表副本 → 本检查器必须报红；
  - 临时删掉一个 §6.2 命令 → 必须报红。
"""
import argparse, os, pathlib, re, shutil, subprocess, sys, tempfile

REPO = pathlib.Path(__file__).resolve().parents[2]
TREE_H = REPO / "lib" / "infrastructure" / "cli" / "command_tree.h"
SESSION_H = REPO / "lib" / "infrastructure" / "cli" / "session_commands.h"
PARSER = REPO / "lib" / "infrastructure" / "cli" / "parser.cpp"
EXIT_H = REPO / "lib" / "infrastructure" / "cli" / "exit_codes.h"

# §6.2 唯一命令树（外部可见命令名 → 期望旗标面）
SPEC_COMMANDS = {
    "normalize": {"--json", "--template", "--help"},
    "mosaic": {"--json", "--template", "--help"},
    "export": {"--json", "--template", "--help"},
    "help": set(),
    "--version": {"--json"},
    "doctor": {"--json"},
    "benchmark": set(),
}
# §6.2 的等价顶层形态：--help/-h 与 help 同义（不写进 help 文本，不是独立命令行）
SPEC_ALIASES = {"--help", "-h"}
# 旧用户命令：这些 token 不得再出现在命令表/help 里（用户命令面已切换）
LEGACY_COMMANDS = [
    "phase1", "phase2", "phase3",
    "phase1 run", "phase1 validate", "phase1 plan", "phase1 inspect",
    "phase2 run", "phase2 validate", "phase2 plan", "phase2 inspect",
    "phase3 run", "phase3 validate", "phase3 plan", "phase3 inspect",
    "config init", "config validate", "config show-effective",
    "modules list", "modules verify", "selftest", "test synthetic",
    "verify", "verify profile", "drizzle", "benchmark cpu",
    "benchmark verify-profile", "hardware inspect", "graph", "run",
]


def read(path):
    return path.read_text(encoding="utf-8") if path.is_file() else ""


def parse_tree_commands(text):
    """从 command_tree.h 的 commands() 表里解析 {"path", bool, {...}} 三元组。"""
    m = re.search(r"commands\(\)\s*\{(.*?)\n    \};", text, re.S)
    if not m:
        return None
    body = m.group(1)
    out = {}
    for mm in re.finditer(r'\{\s*"([^"]+)"\s*,\s*(true|false)\s*,\s*\{([^}]*)\}', body):
        path, pub, flags = mm.group(1), mm.group(2), mm.group(3)
        tokens = set(re.findall(r'"([^"]+)"', flags))
        out[path] = {"public": pub == "true", "tokens": tokens}
    return out


def parse_parser_commands(text):
    """从 parser.cpp 的 kRuleViews 视图确认命令来自 command_tree（无第二份清单）。"""
    return "astrocs::cli::cmd::commands()" in text


def check_static(root=None):
    """静态判据。root 可指向一个临时副本（负例用）。"""
    root = pathlib.Path(root) if root else REPO
    tree_h = root / "lib" / "infrastructure" / "cli" / "command_tree.h"
    parser = root / "lib" / "infrastructure" / "cli" / "parser.cpp"
    exit_h = root / "lib" / "infrastructure" / "cli" / "exit_codes.h"
    errors = []

    tree_text = read(tree_h)
    if not tree_text:
        return ["命令树缺失: lib/infrastructure/cli/command_tree.h"]
    cmds = parse_tree_commands(tree_text)
    if cmds is None or not cmds:
        return ["命令树表不可解析（command_tree.h::commands()）"]

    # [A] 完整性
    missing = sorted(set(SPEC_COMMANDS) - set(cmds))
    extra = sorted(set(cmds) - set(SPEC_COMMANDS) - SPEC_ALIASES)
    if missing:
        errors.append("§6.2 命令缺失: %s" % ", ".join(missing))
    if extra:
        errors.append("命令表含 §6.2 之外的用户命令: %s" % ", ".join(extra))
    for name, want in SPEC_COMMANDS.items():
        if name not in cmds:
            continue
        got = cmds[name]["tokens"]
        for flag in want:
            for tok in (flag, "-h" if flag == "--help" else flag):
                if tok not in got and not (flag == "--help" and "-h" in got):
                    errors.append("%s 缺旗标 %s" % (name, flag))
    # help 文本必须由表生成（无手写副本），且等价形态不得占用 help 行
    if "help_text()" not in tree_text or "out += help_usage(c)" not in tree_text:
        errors.append("help 文本未由命令表生成（禁止手写副本）")
    for alias in SPEC_ALIASES:
        if alias in cmds and cmds[alias]["public"]:
            errors.append("等价形态被写成独立 help 行: %s" % alias)
    if not parse_parser_commands(read(parser)):
        errors.append("parser.cpp 未从 command_tree.h 取命令表（存在第二份清单）")

    # [B] 旧命令不得再登记。
    # 注意: 「benchmark」是 §6.2 的命令名，而「benchmark cpu」「benchmark
    # verify-profile」是旧命令——按完整路径判定，不按首段误伤。
    for legacy in LEGACY_COMMANDS:
        if legacy in SPEC_COMMANDS:
            continue
        if legacy in cmds:
            errors.append("旧用户命令重新登记: %s" % legacy)
        first = legacy.split()[0]
        if first not in SPEC_COMMANDS and first in cmds:
            errors.append("旧命令首段 token 仍在命令表: %s" % first)
    # help 文本里不得出现旧命令行
    for legacy in LEGACY_COMMANDS:
        if re.search(r"^astrocs %s(\s|$)" % re.escape(legacy), tree_text, re.M):
            errors.append("help 文本仍含旧命令: %s" % legacy)
    # 命令实现的源码面：三个子命令之外不得再出现用户命令分发字符串
    cmds_cpp = read(root / "lib" / "infrastructure" / "cli" / "commands.cpp")
    for m in re.finditer(r'if \(joined == "([^"]+)"\)', cmds_cpp):
        name = m.group(1)
        if name not in SPEC_COMMANDS and name not in ("version",):
            errors.append("dispatch 仍分发非 §6.2 命令: %s" % name)

    # [C] 退出码稳定
    exit_text = read(exit_h)
    if "ARGS          = 2" not in exit_text:
        errors.append("exit_codes.h 未定义 ARGS=2（唯一源）")
    if "enum ExitCode" not in exit_text:
        errors.append("exit_codes.h 不是退出码唯一源")
    parser_text = read(parser)
    # 参数/命令错误只能经 ParseError 单一出口（数值退出码在 main.cpp 映射）：
    # parser.cpp 里不得出现字面量 `return 2;`。
    if re.search(r"return\s+2\s*;", parser_text):
        errors.append("parser.cpp 直接写死参数错退出码 2（应经 ParseError 单一出口）")
    if "parse_fail(" not in parser_text:
        errors.append("parser.cpp 无统一参数错误出口 parse_fail")
    main_text = read(root / "lib" / "infrastructure" / "cli" / "main.cpp")
    if "catch (const ParseError&" not in main_text or "astrocs::ARGS" not in main_text:
        errors.append("main.cpp 未把 ParseError 映射到 ARGS(=2) 单一出口")
    return errors


def check_runtime(binary, timeout=120):
    """[D] 运行时 rc 矩阵：新命令 rc=0、旧命令 rc=2 实测。"""
    errors = []
    b = str(binary)
    if not os.path.isfile(b):
        return ["binary 不存在: %s" % b]

    def rc(*args, expect_env=None):
        env = dict(os.environ)
        if expect_env:
            env.update(expect_env)
        try:
            r = subprocess.run([b, *args], capture_output=True, text=True,
                               timeout=timeout, env=env)
            return r.returncode
        except (OSError, subprocess.TimeoutExpired) as e:
            return "run-error:%s" % e

    ok_new = [["help"], ["--version"], ["--version", "--json"],
              ["normalize", "--help"], ["mosaic", "--help"], ["export", "--help"],
              ["normalize", "--template"], ["mosaic", "--template"], ["export", "--template"],
              ["doctor", "--json"]]
    for args in ok_new:
        got = rc(*args)
        if got != 0:
            errors.append("新命令应 rc=0: astrocs %s → rc=%s" % (" ".join(args), got))

    bad_old = [[c, sub] for c in ("phase1", "phase2", "phase3")
               for sub in ("run", "validate", "plan", "inspect")]
    bad_old += [["phase1"], ["phase2"], ["phase3"], ["phase1-run"], ["Phase1"],
                ["version"], ["hardware", "inspect", "--json"],
                ["modules", "list", "--json"], ["selftest", "--json"],
                ["config", "validate", "--config", "/tmp/x.json"],
                ["test", "synthetic", "--group", "all"],
                ["verify", "--run-manifest", "/tmp/x.json", "--json"],
                ["drizzle", "--config", "/tmp/x.json"],
                ["benchmark", "cpu", "--quick"], ["graph", "--preset", "1"],
                ["run", "--phases", "1"]]
    for args in bad_old:
        got = rc(*args)
        if got != 2:
            errors.append("旧命令应 rc=2: astrocs %s → rc=%s" % (" ".join(args), got))

    # 参数/命令错 → 2；输入缺失/格式错 → 3（ASTROCS_DESIGN §6.3 码表）
    bad_args = [["normalize"], ["normalize", "--json"], ["mosaic"], ["export"],
                ["doctor"], ["frobnicate"]]
    missing_input = [["normalize", "--json", "/nonexistent-cfg-cli001.json", "-y"],
                     ["mosaic", "--json", "/nonexistent-cfg-cli001.json", "-y"],
                     ["export", "--json", "/nonexistent-cfg-cli001.json", "-y"]]
    for args in bad_args:
        got = rc(*args)
        if got != 2:
            errors.append("参数/命令错误应 rc=2: astrocs %s → rc=%s" % (" ".join(args), got))
    for args in missing_input:
        got = rc(*args)
        if got != 3:
            errors.append("输入缺失应 rc=3: astrocs %s → rc=%s" % (" ".join(args), got))
    return errors


def self_test():
    """负例自证：故意破坏命令表 → 检查器必须报红。"""
    failures = []
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="cli001_selftest_"))
    try:
        # 副本 = 相关文件域
        for rel in ("lib/infrastructure/cli", "cli"):
            src = REPO / rel
            dst = tmp / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(src, dst, dirs_exist_ok=True)
        base = check_static(tmp)
        if base:
            failures.append("副本基线不为绿（自证无效）: %s" % base[:3])

        # 负例 1: 重新登记旧命令 phase1 run
        p = tmp / "lib" / "infrastructure" / "cli" / "command_tree.h"
        t = p.read_text(encoding="utf-8")
        injected = ('{"phase1 run", true, {"--json", "--config"}},\n'
                    '        {"help",      true, {}},')
        t2 = t.replace('{"help",      true, {}},', injected)
        if t2 == t:
            failures.append("负例注入点未命中（命令表格式变化）")
        else:
            p.write_text(t2, encoding="utf-8")
            if not check_static(tmp):
                failures.append("负例 1 未变红: 重新登记旧命令 phase1 run 仍 PASS")
            p.write_text(t, encoding="utf-8")

        # 负例 2: 删掉 §6.2 命令 mosaic（按行首 token 定位，避免格式假设）
        lines = [ln for ln in t.splitlines(keepends=True)
                 if not ln.lstrip().startswith('{"mosaic"')]
        t3 = "".join(lines)
        if t3 == t:
            failures.append("负例 2 注入点未命中")
        else:
            p.write_text(t3, encoding="utf-8")
            if not check_static(tmp):
                failures.append("负例 2 未变红: 删除 mosaic 仍 PASS")
            p.write_text(t, encoding="utf-8")

        # 恢复后必须重新变绿
        if check_static(tmp):
            failures.append("副本恢复后仍为红（自证不可复跑）")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return failures


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--binary", default=None,
                    help="被测 astrocs 可执行文件（默认自动探测 build/astrocs、build/cli/astrocs）")
    ap.add_argument("--static-only", action="store_true", help="只跑静态判据")
    ap.add_argument("--self-test", action="store_true", help="跑负例自证（必须能红）")
    args = ap.parse_args(argv)

    if args.self_test:
        fails = self_test()
        if fails:
            print("CLI-001_LAYOUT_VIOLATION (self-test):")
            for f in fails:
                print("  " + f)
            return 1
        print("CLI-001_SELFTEST_PASS: 负例(旧命令登记/缺 §6.2 命令)均能变红, 恢复后变绿")
        return 0

    errors = check_static()
    mode = "static"
    if not args.static_only:
        binary = args.binary or os.environ.get("ASTROCS_CLI_BIN")
        if not binary:
            for cand in (REPO / "build" / "astrocs", REPO / "build" / "cli" / "astrocs"):
                if cand.is_file():
                    binary = str(cand)
                    break
        if binary:
            errors += check_runtime(binary)
            mode = "static+runtime(%s)" % binary
        else:
            mode = "static (no binary found; runtime rc matrix skipped)"
    if errors:
        print("CLI-001_LAYOUT_VIOLATION:")
        for e in errors:
            print("  " + e)
        return 1
    tree = parse_tree_commands(read(TREE_H)) or {}
    print("CLI-001_PASS: %d commands (§6.2 tree), legacy phase1/2/3 absent (rc=2 asserted), "
          "exit codes stable [%s]" % (len(tree), mode))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
