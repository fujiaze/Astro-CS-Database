"""P05-003 场景 1.5: 取消测试 (Ctrl+C).

启动 orchestrator.exe stage1 --cancel-on-signal, 在 800ms 后发送 CTRL_C_EVENT,
验证 exit_code=10 (CANCELLED) 且不生成 HISS.

使用 Windows API:
- CreateProcess with CREATE_NEW_CONSOLE (0x10) 给子进程独立控制台
- GenerateConsoleCtrlEvent(CTRL_C_EVENT=0, 0) 发送 Ctrl+C
- 通过 pipe 重定向 stdout/stderr

注意: 必须用 CREATE_NEW_CONSOLE 才能让 GenerateConsoleCtrlEvent 精准发送到子进程的控制台组,
不影响父进程 (Python).
"""
import ctypes
import ctypes.wintypes as w
import json
import os
import subprocess
import sys
import time
from pathlib import Path

# Constants
CREATE_NEW_CONSOLE = 0x00000010
CTRL_C_EVENT = 0
ATTACH_PARENT_PROCESS = -1  # (DWORD)-1

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
kernel32.FreeConsole.argtypes = []
kernel32.FreeConsole.restype = w.BOOL
kernel32.AttachConsole.argtypes = [w.DWORD]
kernel32.AttachConsole.restype = w.BOOL
kernel32.GenerateConsoleCtrlEvent.argtypes = [w.DWORD, w.DWORD]
kernel32.GenerateConsoleCtrlEvent.restype = w.BOOL
kernel32.SetConsoleCtrlHandler.argtypes = [ctypes.c_void_p, w.BOOL]
kernel32.SetConsoleCtrlHandler.restype = w.BOOL


def run_cancel_test(orchestrator, args_list, output_path, ctrl_c_delay_ms=800,
                    wait_after_ctrl_c=10.0):
    """启动 orchestrator, 发送 Ctrl+C, 等待退出, 返回 (exit_code, stdout, stderr)."""
    # 预清输出
    if os.path.exists(output_path):
        os.remove(output_path)

    # 用 subprocess.Popen 启动, 重定向 stdout/stderr
    # CREATE_NEW_CONSOLE 让子进程有独立控制台
    print(f"[cancel] 启动: {orchestrator} {' '.join(args_list)}")
    proc = subprocess.Popen(
        [orchestrator] + args_list,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        creationflags=CREATE_NEW_CONSOLE,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    print(f"[cancel] 子进程 PID={proc.pid}, 等待 {ctrl_c_delay_ms}ms 后发送 Ctrl+C")

    time.sleep(ctrl_c_delay_ms / 1000.0)

    if proc.poll() is not None:
        # 子进程已退出 (太快, 没机会取消)
        print(f"[cancel] 子进程在 Ctrl+C 前已退出, exit={proc.returncode}")
        stdout, stderr = proc.communicate(timeout=5)
        return proc.returncode, stdout, stderr, False

    # 发送 Ctrl+C 到子进程的控制台组:
    # 1. 父进程 FreeConsole
    # 2. AttachConsole(child_pid)
    # 3. SetConsoleCtrlHandler(NULL, TRUE) - 让本进程忽略 Ctrl+C (避免自身被影响)
    # 4. GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0) - 发送到所有附加进程
    # 5. FreeConsole
    # 6. AttachConsole(ATTACH_PARENT_PROCESS) - 重新附加到父控制台

    ok = kernel32.FreeConsole()
    print(f"[cancel] FreeConsole: {ok} (last_err={ctypes.get_last_error()})")

    ok = kernel32.AttachConsole(w.DWORD(proc.pid))
    print(f"[cancel] AttachConsole(child={proc.pid}): {ok} (last_err={ctypes.get_last_error()})")

    # 让本进程忽略 Ctrl+C (避免 Python 自身被取消)
    ok = kernel32.SetConsoleCtrlHandler(None, True)
    print(f"[cancel] SetConsoleCtrlHandler(NULL, TRUE): {ok}")

    # 发送 Ctrl+C 到子进程的控制台组
    ok = kernel32.GenerateConsoleCtrlEvent(w.DWORD(CTRL_C_EVENT), w.DWORD(0))
    print(f"[cancel] GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0): {ok} (last_err={ctypes.get_last_error()})")

    # 等待子进程响应
    print(f"[cancel] 等待子进程退出 (最长 {wait_after_ctrl_c}s)...")
    try:
        stdout, stderr = proc.communicate(timeout=wait_after_ctrl_c)
    except subprocess.TimeoutExpired:
        print("[cancel] WARN: 子进程未在预期时间内退出, 强制 kill")
        proc.kill()
        stdout, stderr = proc.communicate(timeout=5)

    # 恢复本进程的 Ctrl+C 处理
    ok = kernel32.SetConsoleCtrlHandler(None, False)
    print(f"[cancel] SetConsoleCtrlHandler(NULL, FALSE): {ok}")

    # 重新附加到父控制台
    ok = kernel32.FreeConsole()
    print(f"[cancel] FreeConsole (再次): {ok}")
    ok = kernel32.AttachConsole(w.DWORD(ATTACH_PARENT_PROCESS))
    print(f"[cancel] AttachConsole(ATTACH_PARENT_PROCESS): {ok}")

    print(f"[cancel] 子进程退出, exit_code={proc.returncode}")
    return proc.returncode, stdout, stderr, True


def main():
    project_root = Path(r"f:\Astro dev\Astro CS Normalization Database")
    orchestrator = str(project_root / "lib" / "orchestrator" / "cpp" / "orchestrator.exe")
    evidence_dir = project_root / "engineering" / "evidence" / "P05-003"
    scen_log_dir = evidence_dir / "logs" / "s1_5_cancelled"
    scen_log_dir.mkdir(parents=True, exist_ok=True)

    recovery_frame = str(project_root / "testdata" / "Galaxy_Center_T4" / "lights" / "panel1" /
                         "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts")
    recovery_config = str(project_root / "engineering" / "evidence" / "P05-002" / "configs" /
                          "stage1_config_T4.json")
    output_hiss = str(evidence_dir / "hiss" / "s1_5_cancelled.hiss")

    args_list = [
        "stage1",
        "--frame", recovery_frame,
        "--output", output_hiss,
        "--config", recovery_config,
        "--log-level", "INFO",
        "--cancel-on-signal",
    ]

    start_time = time.time()
    exit_code, stdout, stderr, ctrl_c_sent = run_cancel_test(
        orchestrator, args_list, output_hiss,
        ctrl_c_delay_ms=800, wait_after_ctrl_c=15.0
    )
    duration_ms = int((time.time() - start_time) * 1000)

    # 保存日志
    stdout_path = scen_log_dir / "stdout.log"
    stderr_path = scen_log_dir / "stderr.log"
    with open(stdout_path, "w", encoding="utf-8") as f:
        f.write(stdout or "")
    with open(stderr_path, "w", encoding="utf-8") as f:
        f.write(stderr or "")

    output_exists = os.path.exists(output_hiss)

    result = {
        "scenario": "s1_5_cancelled",
        "exit_code": exit_code,
        "duration_ms": duration_ms,
        "output_exists": output_exists,
        "atomicity_ok": not output_exists,
        "ctrl_c_sent": ctrl_c_sent,
        "stdout_path": str(stdout_path),
        "stderr_path": str(stderr_path),
        "output_path": output_hiss,
        "stdout_first_line": (stdout or "").splitlines()[0] if stdout and stdout.strip() else "",
        "stderr_last_line": (stderr or "").splitlines()[-1] if stderr and stderr.strip() else "",
    }

    meta_path = scen_log_dir / "meta.json"
    with open(meta_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    print(f"\n[cancel] 结果: exit_code={exit_code} output_exists={output_exists} "
          f"duration_ms={duration_ms} ctrl_c_sent={ctrl_c_sent}")
    print(f"[cancel] stdout 保存至 {stdout_path}")
    print(f"[cancel] stderr 保存至 {stderr_path}")
    print(f"[cancel] meta 保存至 {meta_path}")

    # 也写一份 JSONL 事件解析结果到 stdout (供主脚本读取)
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
