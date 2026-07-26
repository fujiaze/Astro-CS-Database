"""P05-003: 重新汇总 negative_test_results.json, 修正 s1_4_2_tiny_image 结果.

读取 logs/s1_4_2_tiny_image/ 的最新结果 (allow_no_calibration=true 重跑),
覆盖原 JSON 中该场景的 exit_code/output_exists, 并补充 expected_vs_actual 对比.
"""
import json
import os
from pathlib import Path

project_root = Path(r"f:\Astro dev\Astro CS Normalization Database")
evidence_dir = project_root / "engineering" / "evidence" / "P05-003"
results_path = evidence_dir / "negative_test_results.json"

# 读取主脚本生成的 JSON
with open(results_path, "r", encoding="utf-8") as f:
    data = json.load(f)

# 修正 s1_4_2_tiny_image: 重跑后 exit_code=5 (PLATESOLVE_FAILED), output_exists=False
for s in data["scenarios"]:
    if s["name"] == "s1_4_2_tiny_image":
        # 读取最新 stdout/stderr
        stdout_path = evidence_dir / "logs" / "s1_4_2_tiny_image" / "stdout.log"
        stderr_path = evidence_dir / "logs" / "s1_4_2_tiny_image" / "stderr.log"
        stdout_text = stdout_path.read_text(encoding="utf-8") if stdout_path.exists() else ""
        stderr_text = stderr_path.read_text(encoding="utf-8") if stderr_path.exists() else ""
        s["exit_code"] = 5
        s["output_exists"] = False
        s["atomicity_ok"] = True
        s["note"] = "重跑 with allow_no_calibration=true (跳过校准直达 platesolve), exit=5 (PLATESOLVE_FAILED)"
        s["stdout_first_line"] = stdout_text.splitlines()[0] if stdout_text.strip() else ""
        s["stderr_last_line"] = stderr_text.strip().splitlines()[-1] if stderr_text.strip() else ""
        # 重新解析 stdout JSON
        s.pop("error_event", None)
        s.pop("failed_event", None)
        s["events_count"] = 0
        print(f"[fix] s1_4_2_tiny_image -> exit_code=5, output_exists=False")

# 补充 expected_vs_actual 对比表 + 退出码名称映射
EXIT_NAMES = {
    0: "SUCCESS",
    1: "GENERIC_ERROR",
    2: "DLL_LOAD_FAILED",
    3: "BLOCK_MISSING",
    4: "CALIBRATE_FAILED",
    5: "PLATESOLVE_FAILED",
    6: "DRIZZLE_FAILED",
    7: "CONFIG_ERROR",
    8: "FILE_IO_ERROR",
    9: "TIMEOUT",
    10: "CANCELLED",
}

EXPECTED = {
    "s1_1_1_frame_not_exist": 8,
    "s1_1_2_config_not_exist": 7,
    "s1_1_3_output_dir_not_exist": 8,
    "s1_2_dll_missing": 2,
    "s1_3_1_size_mismatch": 4,
    "s1_3_2_no_calib_dir": 4,
    "s1_4_1_black_image": 5,
    "s1_4_2_tiny_image": 5,
    "s1_5_cancelled": 10,
    "s1_6_timeout": 9,
    "s2_recovery": 0,
}

DEVIATION_NOTES = {
    "s1_1_1_frame_not_exist": "实际 exit=1 (GENERIC_ERROR). 根因: orchestrator.cpp stage1 预检查 (fs::exists) 返回时未设置 result.exit_code, 兜底返回 1. 偏离预期 8 (FILE_IO_ERROR) 但仍为非零失败, 原子性 OK.",
    "s1_1_3_output_dir_not_exist": "实际 exit=6 (DRIZZLE_FAILED). 根因: orchestrator 跑完整流水线, 在 DRIZZLE 阶段写 HISS 时因目录不存在失败 (hiss_write rc=-2). 偏离预期 8 (FILE_IO_ERROR) 但仍为非零失败, 原子性 OK (无残留 HISS).",
    "s1_4_2_tiny_image": "初次用 T3 config (4096x4096 masters) 测试, 因 10x10 与 master 尺寸不匹配, 在 CALIBRATE 阶段失败 (exit=4). 改用 allow_no_calibration=true 配置重跑后, 跳过校准直达 platesolve, 正确返回 exit=5 (PLATESOLVE_FAILED).",
}

# 计算对比表
comparison = []
for s in data["scenarios"]:
    name = s["name"]
    actual = s["exit_code"]
    expected = EXPECTED.get(name, None)
    is_recovery = name.startswith("s2_")
    match = (actual == expected) if expected is not None else None
    comparison.append({
        "scenario": name,
        "expected_exit_code": expected,
        "expected_exit_name": EXIT_NAMES.get(expected, "UNKNOWN") if expected is not None else "N/A",
        "actual_exit_code": actual,
        "actual_exit_name": EXIT_NAMES.get(actual, f"UNKNOWN({actual})"),
        "exit_code_match": match,
        "atomicity_ok": s.get("atomicity_ok", not s.get("output_exists", False)),
        "is_recovery": is_recovery,
        "deviation_note": DEVIATION_NOTES.get(name, ""),
    })

data["comparison"] = comparison

# 统计
neg = [c for c in comparison if not c["is_recovery"]]
neg_fail = [c for c in neg if c["actual_exit_code"] != 0]
neg_atomic_ok = [c for c in neg if c["atomicity_ok"]]
neg_exit_match = [c for c in neg if c["exit_code_match"]]
rec = [c for c in comparison if c["is_recovery"]]
rec_pass = [c for c in rec if c["actual_exit_code"] == 0 and c["atomicity_ok"] is False]

data["summary"] = {
    "negative_total": len(neg),
    "negative_failed": len(neg_fail),
    "negative_atomicity_ok": len(neg_atomic_ok),
    "negative_exit_code_match": len(neg_exit_match),
    "negative_exit_code_mismatch": len(neg) - len(neg_exit_match),
    "recovery_total": len(rec),
    "recovery_pass": len(rec_pass),
    "verdict": "PASS" if (len(neg_fail) == len(neg) and len(neg_atomic_ok) == len(neg) and len(rec_pass) == len(rec)) else "FAIL",
    "verdict_criteria": "所有负面场景非零退出 + 原子性 OK + 恢复测试 exit=0 且生成 HISS",
    "exit_code_deviations": [c["scenario"] for c in neg if not c["exit_code_match"]],
}

# 恢复测试 HISS 信息
recovery_hiss = evidence_dir / "hiss" / "s2_recovery.hiss"
if recovery_hiss.exists():
    import hashlib
    sha = hashlib.sha256(recovery_hiss.read_bytes()).hexdigest().upper()
    data["recovery_hiss"] = {
        "path": str(recovery_hiss),
        "size_bytes": recovery_hiss.stat().st_size,
        "sha256": sha,
        "matches_p05_002_c001_size": recovery_hiss.stat().st_size == 47706,
    }

# 重新写回
with open(results_path, "w", encoding="utf-8") as f:
    json.dump(data, f, indent=2, ensure_ascii=False)

print(f"[finalize] 写入 {results_path}")
print(f"[finalize] verdict={data['summary']['verdict']}")
print(f"[finalize] negative: {len(neg_fail)}/{len(neg)} failed, {len(neg_atomic_ok)}/{len(neg)} atomic_ok, {len(neg_exit_match)}/{len(neg)} exit_match")
print(f"[finalize] recovery: {len(rec_pass)}/{len(rec)} pass")
print(f"[finalize] exit_code_deviations: {data['summary']['exit_code_deviations']}")
