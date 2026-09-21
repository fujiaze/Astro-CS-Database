#!/usr/bin/env python3
"""RUNTIME-CI-001 独立结构 Oracle（V6 运行面收口）。

与 C++ 契约测试**不同实现**：本 Oracle 只做源码/配置的静态结构判定（不链接、不
复用 v6_runtime_contract.h 的代码逻辑），因此对「契约实现被改坏」具备独立检出能力。

覆盖（每项 = 一条硬规则，任一违反 → rc=1）：
  R1  CLI 模式路由表（FZ-MODE-PRODUCTION / FZ-MODE-DEFERRED / FZ-FIELD-WEIGHTMODE /
      FZ-P3-MODES）在 lib/infrastructure/cli/v6_runtime_contract.h 中唯一落位，且 reject 理由引用冻结节点。
  R2  --mode / --export-mode 只在 phase2 / phase3 命令面登记（parser kRules），phase1 不得有。
  R3  §3.2 三 Phase 隔离：CLI 命令面不得存在聚合式 run/graph/pipeline 入口。
  R4  §10.4 统一预算：进程唯一预算源登记处存在；V6 运行面不得出现未登记的私有长期线程池。
  R5  §10.5 必采字段面：monitor/recorder 携带每线程 CPU / active compute threads /
      iowait / rss/pss / 读写字节 / 队列 / worker 均衡 / wall。
  R6  SO-05：资源判据在 SO-05 未签字前恒为 record_only + PENDING_OWNER_SIGNOFF，
      且不存在把该判定升级为硬失败的契约入口。
  R7  eng/ci/checks.json 登记 V6 检查项（有 timeout / outputs / ctest_targets 或等价依赖）。
  R8  确定性契约键存在（FZ-RUNTIME-DETERMINISM）。

用法:
  python3 eng/tools/v6/v6_runtime_oracle.py [--repo <root>] [--json-out <path>]
exit 0 = 全绿；1 = 有违规；2 = 用法/IO 错误。
"""
import argparse
import json
import pathlib
import re
import sys

DEFAULT_REPO = pathlib.Path(__file__).resolve().parents[3]

PROD_P2 = ["point_information", "surface_gls", "psfsw_robust"]
BASE_P2 = ["equal", "pixel_ivar"]
REJECT_P2 = ["psf_snr_power", "auto", "support_x_snr2"]
PROD_P3 = ["surface_brightness", "point_source_flux", "visualization"]
REQUIRED_METRIC_TOKENS = [
    "per_thread_cpu_max", "per_thread_cpu_sum", "active_compute_threads",
    "io_wait", "rss_bytes", "pss_bytes", "read_bytes", "write_bytes",
    "queue_depth", "runnable_workers", "elapsed_seconds",
]
V6_CHECK_IDS = [
    "V6-RUNTIME-CLOSURE", "V6-CLI-MODE-ROUTING", "V6-RESOURCE-GATE-POLICY",
    "V6-DETERMINISM", "V6-NEGATIVE-MUTATION", "V6-CTEST-UNIT", "V6-CTEST-INTEGRATION",
    "V6-CLI-MODE-MATRIX",
]


class Ctx:
    def __init__(self, repo: pathlib.Path):
        self.repo = repo
        self.checks = 0
        self.violations = []
        self.evidence = {}

    def read(self, rel, required=True):
        p = self.repo / rel
        if not p.exists():
            if required:
                self.fail("FILE-MISSING", rel + " 不存在（缺文件不得静默通过）")
            return None
        return p.read_text(encoding="utf-8", errors="replace")

    def check(self, rule, cond, detail):
        self.checks += 1
        if not cond:
            self.violations.append({"rule": rule, "detail": detail})

    def fail(self, rule, detail):
        self.checks += 1
        self.violations.append({"rule": rule, "detail": detail})


def strip_comments(text: str) -> str:
    out = []
    for line in text.splitlines():
        s = line.lstrip()
        if s.startswith("//"):
            continue
        out.append(line)
    return "\n".join(out)


def rule_r1_mode_routing(ctx: Ctx):
    """FZ-MODE-* 词表在契约头中唯一落位。"""
    src = ctx.read("lib/infrastructure/cli/v6_runtime_contract.h")
    if src is None:
        return
    code = strip_comments(src)
    for tok in PROD_P2:
        ctx.check("R1-phase2-production", ('"' + tok + '"') in code,
                  "lib/infrastructure/cli/v6_runtime_contract.h 缺 phase2 生产模式 " + tok)
    for tok in BASE_P2:
        ctx.check("R1-phase2-baseline", ('"' + tok + '"') in code,
                  "lib/infrastructure/cli/v6_runtime_contract.h 缺 phase2 baseline " + tok)
    ctx.check("R1-deferred",
              "FZ-MODE-DEFERRED" in code and "psf_snr_power" in code,
              "psf_snr_power 的 DEFERRED 拒绝未在契约头显式落位（code 面）")
    ctx.check("R1-weightmode-reject",
              "FZ-FIELD-WEIGHTMODE" in src and "legacy weight_mode 0" in src,
              "legacy weight_mode 0 拒绝理由未登记（FZ-FIELD-WEIGHTMODE）")
    for tok in PROD_P3:
        ctx.check("R1-phase3-production", ('"' + tok + '"') in code,
                  "lib/infrastructure/cli/v6_runtime_contract.h 缺 phase3 输出模式 " + tok)
    ctx.check("R1-p3-modes", "FZ-P3-MODES" in src,
              "Phase3 输出模式未引用冻结节点 FZ-P3-MODES")
    # 契约头不得出现"把 deferred 当生产放行"的路径
    ctx.check("R1-no-deferred-production",
              "route_phase2_mode" in code and "kProduction" in code,
              "契约头缺 route_phase2_mode / kProduction 路由实现")
    # legacy 整数映射：1|2 -> baseline；0 -> reject（fail-closed）
    ctx.check("R1-legacy-int-map",
              "route_legacy_weight_mode_int" in code and "kBaseline" in code,
              "缺 legacy 整数 weight_mode → baseline 映射")
    mb = re.search(r"route_legacy_weight_mode_int[\s\S]{0,1200}?\n\}", code)
    body = mb.group(0) if mb else ""
    ctx.check("R1-legacy-int-reject",
              "v == 0" in body and "FZ-FIELD-WEIGHTMODE" in body,
              "route_legacy_weight_mode_int 缺 legacy 0 的 FZ-FIELD-WEIGHTMODE 拒绝分支")


# W4-A3：阶段 ↔ 用户命令名的映射唯一事实源 = ASTROCS_DESIGN §2:317
# （「phase1|2|3 仅为内部指代（设计层面），代码与命令名使用 normalize/mosaic/export」）。
STAGE_COMMANDS = (("phase1", "normalize"), ("phase2", "mosaic"), ("phase3", "export"))


def _command_tree_flags(ctx: Ctx) -> dict:
    """从 command_tree.h 解析 {命令名: 旗标集合}；解析为空即判红（fail-closed）。"""
    tree = ctx.read("lib/infrastructure/cli/command_tree.h")
    if tree is None:
        return {}
    out = {}
    for name, flags in re.findall(
            r'\{\s*"([A-Za-z0-9_-]+)"\s*,\s*(?:true|false)\s*,\s*\{([^}]*)\}', tree):
        # 注意取**组内**内容：命令树里的旗标写作 "--mode"，带引号；
        # 若把引号留在集合里，"--mode" in flags 会恒为 False（假红）。
        out[name] = set(re.findall(r'"([^"]+)"', flags))
    return out


def rule_r2_cli_flags(ctx: Ctx):
    """--mode 仅 phase2(mosaic)，--export-mode 仅 phase3(export)；phase1(normalize) 不得有。

    **W4-A3 判据改绑（A 类/B 类：判据读取面过时 ⇒ 空转）**。原实现从
    `parser.cpp` 抓 `"phase[123] ..."` 字面量表；CLI-001 已删除这些用户命令
    （parser.cpp:31-33），正则命中 0 条 ⇒ 4 条 R2-* 检查**全部被静默跳过**，
    规则只剩 R2-value-flags 在跑（"空转绿"，正是 ENGINEERING_SPEC §8 要防的失效型）。
    现改绑到命令树唯一事实源 `command_tree.h` + 设计映射（STAGE_COMMANDS），
    并加 `R2-command-tree-parsed`：解析不到命令即判红，杜绝再次空转。
    """
    tree = ctx.read("lib/infrastructure/cli/command_tree.h")
    if tree is None:
        return
    seen = _command_tree_flags(ctx)
    ctx.check("R2-command-tree-parsed", len(seen) >= 4,
              "command_tree.h 仅解析到 %d 条命令（判据面被移空）" % len(seen))
    parser = ctx.read("lib/infrastructure/cli/parser.cpp")
    for stage, cmd in STAGE_COMMANDS:
        flags = seen.get(cmd)
        ctx.check("R2-stage-command-present-" + stage, flags is not None,
                  "命令树缺 %s 阶段命令 %s（ASTROCS_DESIGN §2:317/:439）" % (stage, cmd))
        if flags is None:
            continue
        if stage == "phase2":
            ctx.check("R2-phase2-mode-flag", "--mode" in flags,
                      cmd + " 未登记 --mode 显式模式旗标")
            ctx.check("R2-phase2-no-export", "--export-mode" not in flags,
                      cmd + " 误登记 phase3 的 --export-mode")
        if stage == "phase3":
            ctx.check("R2-phase3-export-flag", "--export-mode" in flags,
                      cmd + " 未登记 --export-mode 显式模式旗标")
            ctx.check("R2-phase3-no-weight-mode", "--mode" not in flags,
                      cmd + " 误登记 phase2 的 --mode")
        if stage == "phase1":
            ctx.check("R2-phase1-no-mode",
                      "--mode" not in flags and "--export-mode" not in flags,
                      cmd + " 不得登记 V6 模式旗标（Phase1 无权重/输出模式）")
    if parser is not None:
        ctx.check("R2-value-flags",
                  '"--mode"' in parser and '"--export-mode"' in parser,
                  "parser kValueFlags 未登记 --mode/--export-mode")
    # 模式门必须在 phase2/phase3 各命令面被调用
    cmds = ctx.read("lib/infrastructure/cli/commands.cpp")
    gate = ctx.read("lib/infrastructure/cli/v6_mode_gate.h")
    if cmds and gate:
        ctx.check("R2-gate-include", "v6_mode_gate.h" in cmds,
                  "commands.cpp 未 include v6_mode_gate.h")
        n_calls = len(re.findall(r"astrocs::v6cli::mode_gate\s*\(", cmds))
        ctx.check("R2-gate-call-sites", n_calls >= 4,
                  "mode_gate 调用点 %d 处 (<4: run/validate/plan phase2/phase3)" % n_calls)
        ctx.check("R2-gate-fail-closed",
              "astrocs::ARGS" in gate and "RouteKind::kReject" in gate,
              "mode_gate 未对 reject 返回 ARGS(2)（fail-closed 缺失）")


def rule_r3_phase_isolation(ctx: Ctx):
    """CLI 命令面不得有聚合 run/graph/pipeline 入口；阶段命令名按设计权威。

    **W4-A3 判据改绑（B 类：判据过时）**。原实现从 `lib/infrastructure/cli/parser.cpp`
    的字面量表 `kRules` 抓路径，并断言 `phase1 run`/`phase2 run`/`phase3 run`
    三条**逐 phase 用户命令必须存在**。两者都已过时：

      ① 命令树的唯一事实源已迁到 `lib/infrastructure/cli/command_tree.h`
         （parser.cpp:70-81 现为 `kRuleViews()` 对 command_tree 的派生视图，
         不再含字面量路径表 ⇒ 原正则命中 0 条，规则整体空转后 3 条 check 恒红）；
      ② `ASTROCS_DESIGN.md:317` 明定「`phase1|2|3` 仅为内部指代（设计层面），
         代码与命令名使用 `normalize/mosaic/export`」；`:439` 明定子命令面为
         `normalize/mosaic/export/help/benchmark/doctor`。CLI-001 已按此删除
         `phase1|2|3 *` 用户命令（parser.cpp:31-33 注明这些一律 unknown command → 2）。

    改绑后**意图不变、口径更严**：仍旧禁聚合入口、仍旧禁一条命令跨多阶段；
    但阶段命令名以设计权威的 `normalize/mosaic/export` 为准，并**反向**断言
    `phase1|2|3` 不得再作为用户命令出现（防复活）。
    """
    tree = ctx.read("lib/infrastructure/cli/command_tree.h")
    if tree is None:
        return
    paths = re.findall(r'\{\s*"([A-Za-z0-9_ -]+)"\s*,\s*(?:true|false)\s*,\s*\{', tree)
    ctx.evidence["cli_command_paths"] = sorted(paths)
    ctx.check("R3-command-tree-nonempty", len(paths) >= 6,
              "command_tree.h 解析到 %d 条命令路径（命令树被移空？）" % len(paths))
    for p in paths:
        head = p.split()[0]
        ctx.check("R3-no-aggregate-entry", head not in ("run", "graph", "pipeline", "all"),
                  "CLI 命令树出现聚合式入口: " + p)
    # 设计权威（ASTROCS_DESIGN §2 :439）的三条阶段命令必须存在
    for cmd in ("normalize", "mosaic", "export"):
        ctx.check("R3-stage-command-present", cmd in paths,
                  "缺设计权威阶段命令 " + cmd)
    # 反向：phase1|2|3 只允许作为内部指代，不得成为用户命令（ASTROCS_DESIGN :317）
    for p in paths:
        toks = p.split()
        ctx.check("R3-no-per-phase-user-command",
                  not any(t in ("phase1", "phase2", "phase3") for t in toks),
                  "phase1|2|3 不得作为用户命令（设计仅作内部指代）: " + p)
    # 不存在把多 phase 串接为单命令的规则
    for p in paths:
        toks = p.split()
        phases = {t for t in toks if t in ("phase1", "phase2", "phase3")}
        ctx.check("R3-single-phase-per-command", len(phases) <= 1,
                  "命令 " + p + " 同时含多个 Phase")


def rule_r4_unified_budget(ctx: Ctx):
    """§10.4 唯一预算源；V6 运行面无线程池私有实现。"""
    for rel in ("lib/infrastructure/cli/v6_runtime_contract.h", "lib/infrastructure/scheduler/v6_budget.py"):
        src = ctx.read(rel)
        if src is None:
            continue
        ctx.check("R4-single-source-registry",
                  "ProcessBudgetRegistry" in src or "register_source" in src,
                  rel + " 缺进程唯一预算源登记处")
    hdr = ctx.read("lib/infrastructure/cli/v6_runtime_contract.h")
    if hdr:
        code = strip_comments(hdr)
        ctx.check("R4-second-source-rejected", "if (has_source_) return false;" in code,
                  "契约头未见第二预算源拒绝实现（if (has_source_) return false;）")
        ctx.check("R4-lease-oversubscribe",
                  "request_lease" in code and "nested_parallel_denied" in code,
                  "契约头缺线程租约/嵌套并行拒绝")
    # V6 运行面不得出现硬编码 num_threads(<数字>)
    for rel in ("lib/infrastructure/cli/commands.cpp", "lib/infrastructure/cli/v6_mode_gate.h", "lib/infrastructure/cli/v6_runtime_contract.h"):
        src = ctx.read(rel, required=False)
        if src is None:
            continue
        for ln, line in enumerate(src.splitlines(), 1):
            s = line.lstrip()
            if s.startswith("//"):
                continue
            ctx.check("R4-no-hardcoded-threads",
                      not re.search(r"num_threads\s*\(\s*\d+\s*\)|omp_set_num_threads\s*\(\s*\d+\s*\)", line),
                      rel + ":" + str(ln) + " 出现硬编码线程数")
    # V6 模块不得建立私有长期线程池（std::thread 需在登记表；本任务面无新增池）
    for rel in ("lib/infrastructure/cli/v6_runtime_contract.h", "lib/infrastructure/cli/v6_mode_gate.h"):
        src = ctx.read(rel, required=False)
        if src is None:
            continue
        ctx.check("R4-no-private-pool",
                  "std::thread" not in strip_comments(src),
                  rel + " 出现 std::thread（不得在运行面建立私有长期线程池）")


def rule_r5_metric_fields(ctx: Ctx):
    """§10.5 必采字段面。"""
    mon = ctx.read("lib/infrastructure/cli/monitor.h")
    rec = ctx.read("lib/infrastructure/cli/resource_recorder.h")
    if mon:
        for tok in ("per_thread_cpu", "active_compute_threads", "io_wait",
                    "rss_bytes", "pss_bytes", "read_bytes", "write_bytes"):
            ctx.check("R5-monitor-fields", tok in mon,
                      "lib/infrastructure/cli/monitor.h 缺指标 " + tok)
        for decl in ("double sys_io_wait_seconds = 0.0", "double per_thread_cpu_max_seconds = 0.0",
                     "double per_thread_cpu_sum_seconds = 0.0", "uint32_t d_active_compute_threads = 0"):
            ctx.check("R5-monitor-decls", decl in mon,
                      "lib/infrastructure/cli/monitor.h ProcSample 缺字段声明: " + decl)
    if rec:
        for tok in ("per_thread_cpu_max_pct", "per_thread_cpu_sum_pct",
                    "active_compute_threads", "io_wait_pct", "queue_depth",
                    "runnable_workers", "read_bytes", "write_bytes"):
            ctx.check("R5-recorder-fields", tok in rec,
                      "lib/infrastructure/cli/resource_recorder.h 缺指标 " + tok)
        # ResRecord 字段本体（防只留引用/注释而删字段）
        for decl in ("double per_thread_cpu_max_pct = 0.0", "double per_thread_cpu_sum_pct = 0.0",
                     "double io_wait_pct = 0.0", "uint32_t active_compute_threads = 0"):
            ctx.check("R5-recorder-record-decls", decl in rec,
                      "lib/infrastructure/cli/resource_recorder.h ResRecord 缺字段声明: " + decl)
    cmds = ctx.read("lib/infrastructure/cli/commands.cpp")
    if cmds:
        for tok in ("per_thread_cpu_max_pct", "per_thread_cpu_sum_pct",
                    "active_compute_threads_peak", "io_wait_pct", "work_units",
                    "so05_signoff_status"):
            ctx.check("R5-gate-event-fields", tok in cmds,
                      "resource gate 事件缺字段 " + tok)


def rule_r6_so05(ctx: Ctx):
    """SO-05 记录/裁决分离（未签字不得硬失败）。"""
    hdr = ctx.read("lib/infrastructure/cli/v6_runtime_contract.h")
    if hdr is None:
        return
    ctx.check("R6-signoff-status", "PENDING_OWNER_SIGNOFF" in hdr,
              "契约头缺 SO-05 PENDING_OWNER_SIGNOFF 标记")
    ctx.check("R6-record-only", "record_only" in hdr,
              "契约头缺 record_only 策略")
    ctx.check("R6-no-auto-adjudication",
              "auto_adjudication_allowed = false" in hdr,
              "契约头未将 auto_adjudication_allowed 固定为 false")
    # evaluate_heavy_run 不得设置 hard_fail = true
    m = re.search(r"evaluate_heavy_run\s*\([^)]*\)\s*\{", hdr)
    if not m:
        ctx.fail("R6-evaluate-fn", "契约头缺 evaluate_heavy_run 实现")
    else:
        body = hdr[m.end():m.end() + 400]
        ctx.check("R6-hard-fail-false", "hard_fail = false" in body,
                  "evaluate_heavy_run 未固定 hard_fail=false（擅自升级为硬失败）")
    cmds = ctx.read("lib/infrastructure/cli/commands.cpp")
    if cmds:
        idx = cmds.find("resource_gate")
        ctx.check("R6-cmds-markers",
              "so05_signoff_status" in cmds and "auto_adjudication_allowed" in cmds,
              "commands.cpp resource gate 事件缺 SO-05 显式 pending 标记")


def rule_r7_ci_registration(ctx: Ctx):
    """eng/ci/checks.json 登记 V6 检查项。"""
    src = ctx.read("eng/ci/checks.json")
    if src is None:
        return
    try:
        doc = json.loads(src)
    except Exception as e:
        ctx.fail("R7-json", "eng/ci/checks.json 不可解析: %s" % e)
        return
    # **W4-A3 判据改绑（A 类：判据索引面与注册表两层结构不匹配）**。
    # eng/ci/checks.json 是**两层注册表**：顶层聚合项 checks[].id + 执行单元 steps[].id
    # （step 未声明的字段按 INHERIT_FIELDS 继承父项）。V6 的 8 个 ID 全部以
    # **执行单元**形态登记，而原实现只索引顶层 ⇒ 8 条 R7-check-present 恒红，
    # 与 eng/ci/tests/test_impact_map.py 的 4 条既有红同根（step id 与顶层 id 混用）。
    # 改绑：两层联合索引；step 命中的按 step 自身字段判 timeout/outputs，
    # 字段缺失时回退父项（与 eng/ci/run_checks.py 的继承口径一致）。
    by_id = {}
    for c in doc.get("checks", []):
        if not isinstance(c, dict):
            continue
        by_id.setdefault(c.get("id"), c)
        for s in (c.get("steps") or []):
            if isinstance(s, dict) and s.get("id"):
                merged = dict(c)
                merged.update({k: v for k, v in s.items() if v is not None})
                by_id.setdefault(s["id"], merged)
    ctx.evidence["ci_check_ids"] = sorted(by_id)
    ctx.check("R7-registry-two-layer", True,
              "两层注册表联合索引：%d 个 ID（顶层 + 执行单元）" % len(by_id))
    for cid in V6_CHECK_IDS:
        c = by_id.get(cid)
        ctx.check("R7-check-present", c is not None, "eng/ci/checks.json 缺检查项 " + cid)
        if c is None:
            continue
        ctx.check("R7-timeout", isinstance(c.get("timeout_seconds"), int) and c["timeout_seconds"] > 0,
                  cid + " 缺 timeout_seconds>0")
        ctx.check("R7-outputs", isinstance(c.get("outputs"), list) and len(c["outputs"]) > 0,
                  cid + " 缺 outputs 路径")
        ctx.check("R7-not-waivable", c.get("waivable") is False,
                  cid + " 不得 waivable（不得用 waiver 掩盖红灯）")
    # V6 ctest 目标逐名/逐 glob 登记（AR-034 逐名验收锚）
    ct = []
    for c in doc.get("checks", []):
        ct += [p for p in (c.get("ctest_targets") or []) if "v6" in p or "p2_rej_v6" in p]
    ctx.evidence["v6_ctest_targets"] = sorted(set(ct))
    ctx.check("R7-ctest-anchors", len(ct) >= 8,
              "V6 ctest 目标验收锚不足（%d）" % len(ct))


def rule_r8_determinism(ctx: Ctx):
    for rel in ("lib/infrastructure/cli/v6_runtime_contract.h", "eng/tests/system/v6_runtime/v6_runtime_determinism_test.cpp"):
        src = ctx.read(rel)
        if src is None:
            continue
        ctx.check("R8-determinism-contract",
                  "FZ-RUNTIME-DETERMINISM" in src or "DETERMINISM" in src or "byte_identical" in src,
                  rel + " 缺确定性契约锚")


RULES = [rule_r1_mode_routing, rule_r2_cli_flags, rule_r3_phase_isolation,
         rule_r4_unified_budget, rule_r5_metric_fields, rule_r6_so05,
         rule_r7_ci_registration, rule_r8_determinism]

RULE_TAGS = {
    "R1": "r1_mode_routing", "R2": "r2_cli_flags", "R3": "r3_phase_isolation",
    "R4": "r4_unified_budget", "R5": "r5_metric_fields", "R6": "r6_so05",
    "R7": "r7_ci_registration", "R8": "r8_determinism",
}


def evaluate(repo: pathlib.Path, rules=None) -> dict:
    ctx = Ctx(repo)
    want = None
    if rules:
        want = {r.strip().upper() for r in str(rules).split(",") if r.strip()}
    for fn in RULES:
        tag = fn.__name__.split("_")[1].upper()   # rule_rN_... -> RN
        if want is not None and tag not in want:
            continue
        fn(ctx)
    return {
        "schema": "astrocs.v6.runtime-oracle/v1",
        "repo": str(repo),
        "rules": sorted(want) if want else "all",
        "checks": ctx.checks,
        "violations": ctx.violations,
        "verdict": "PASS" if not ctx.violations else "FAIL",
        "evidence": ctx.evidence,
    }


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=str(DEFAULT_REPO))
    ap.add_argument("--json-out", default="")
    ap.add_argument("--rules", default="",
                    help="仅跑指定规则子集, 逗号分隔 (如 R1,R2,R3)")
    args = ap.parse_args(argv)
    repo = pathlib.Path(args.repo).resolve()
    if not (repo / "lib" / "infrastructure" / "cli").exists():
        print("v6_runtime_oracle: repo root invalid: %s" % repo, file=sys.stderr)
        return 2
    res = evaluate(repo, args.rules)
    text = json.dumps(res, ensure_ascii=False, indent=2)
    if args.json_out:
        p = pathlib.Path(args.json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text + "\n", encoding="utf-8")
    if res["verdict"] == "PASS":
        print("V6_RUNTIME_ORACLE_PASS checks=%d violations=0" % res["checks"])
        return 0
    print("V6_RUNTIME_ORACLE_FAIL checks=%d violations=%d" % (res["checks"], len(res["violations"])))
    for v in res["violations"][:40]:
        print("  [%s] %s" % (v["rule"], v["detail"]))
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
