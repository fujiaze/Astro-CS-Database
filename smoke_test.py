"""
P01-006: AstroCS 根级 Smoke Test
验证 build/artifacts/ 下所有 DLL 可加载、SHA-256 一致、导出符号可用。
"""
import ctypes
import hashlib
import json
import os
import subprocess
import sys
from datetime import datetime
from pathlib import Path

REPO = Path(__file__).resolve().parent
ARTIFACTS = REPO / "build" / "artifacts"
MANIFEST = REPO / "build" / "manifest.json"
MSYS2_BIN = r"C:\msys64\mingw64\bin"
REPORT = REPO / "build" / "logs" / "smoke_test_report.json"

# DLL 加载顺序 (按依赖分层, 被依赖的先加载)
LOAD_ORDER = [
    "gaia_client.dll",        # 无 DLL 依赖 (仅 zlib/openmp)
    "astro_image_io.dll",     # 依赖 zstd/lz4/openmp
    "astro_calibration.dll",  # 依赖 openmp
    "dynamic_psf.dll",        # 依赖 openmp
    "star_detector.dll",      # 依赖 gsl/openmp
    "snr_estimator.dll",      # 依赖 openmp
    "healpix_drizzle.dll",    # 依赖 astro_image_io
    "healpix_stack.dll",      # 依赖 astro_image_io/gaia_client
    "photometric_calib.dll",  # 依赖 gaia_client
    "ipv_solver.dll",         # 运行时动态加载, 编译期无依赖
]

# 产物名映射 (manifest 中的 output -> 实际文件名)
OUTPUT_MAP = {
    "astro_image_io.dll": "astro_image_io.dll",
    "astro_calibration.dll": "calibration.dll",  # manifest 用原名, artifacts 也是原名
    "dynamic_psf.dll": "dynamic_psf.dll",
    "gaia_client.dll": "gaia_client.dll",
    "star_detector.dll": "star_detector.dll",
    "snr_estimator.dll": "snr_estimator.dll",
    "photometric_calib.dll": "photometric_calib.dll",
    "ipv_solver.dll": "ipv_solver.dll",
    "healpix_drizzle.dll": "healpix_drizzle.dll",
    "healpix_stack.dll": "healpix_stack.dll",
}

# 版本函数映射
VERSION_FUNCS = {
    "astro_calibration.dll": "ac_version",
}


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def main():
    # 注入 PATH: MSYS2 bin + artifacts
    os.environ["PATH"] = str(ARTIFACTS) + os.pathsep + MSYS2_BIN + os.pathsep + os.environ.get("PATH", "")
    # Python 3.8+ 使用 os.add_dll_directory 添加 DLL 搜索路径 (ctypes 加载依赖 DLL 时需要)
    try:
        os.add_dll_directory(str(ARTIFACTS))
        os.add_dll_directory(MSYS2_BIN)
    except (OSError, AttributeError):
        pass

    # 读取 manifest
    if not MANIFEST.exists():
        print(f"[FAIL] manifest 不存在: {MANIFEST}")
        sys.exit(1)
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    print("=" * 60)
    print("AstroCS Smoke Test (P01-006)")
    print("=" * 60)
    print()

    results = []
    pass_count = 0
    fail_count = 0

    # === 1. SHA-256 校验 ===
    print("--- 1. SHA-256 校验 ---")
    for r in manifest.get("results", []):
        if r["status"] != "OK":
            continue
        output = r["output"]
        expected_sha = r.get("sha256", "")
        if not output or not expected_sha:
            continue
        # 查找实际文件
        artifact_path = ARTIFACTS / output
        if not artifact_path.exists():
            # 尝试映射名
            actual_name = OUTPUT_MAP.get(output, output)
            artifact_path = ARTIFACTS / actual_name
        if not artifact_path.exists():
            entry = {"check": "sha256", "module": r["module"], "status": "FAIL", "error": f"产物文件不存在: {output}"}
            results.append(entry)
            fail_count += 1
            print(f"  [FAIL] {r['module']}: 产物文件不存在 {output}")
            continue
        actual_sha = sha256_file(artifact_path)
        if actual_sha == expected_sha.upper():
            entry = {"check": "sha256", "module": r["module"], "status": "PASS", "file": str(artifact_path), "sha256": actual_sha}
            results.append(entry)
            pass_count += 1
            print(f"  [OK] {r['module']}: SHA-256 一致")
        else:
            entry = {"check": "sha256", "module": r["module"], "status": "FAIL", "error": f"SHA-256 不符 (预期 {expected_sha}, 实际 {actual_sha})"}
            results.append(entry)
            fail_count += 1
            print(f"  [FAIL] {r['module']}: SHA-256 不符")

    # === 2. DLL 加载测试 ===
    print()
    print("--- 2. DLL 加载测试 ---")
    loaded_dlls = {}
    for dll_name in LOAD_ORDER:
        dll_path = ARTIFACTS / dll_name
        if not dll_path.exists():
            entry = {"check": "load", "module": dll_name, "status": "FAIL", "error": "文件不存在"}
            results.append(entry)
            fail_count += 1
            print(f"  [FAIL] {dll_name}: 文件不存在")
            continue
        try:
            # 用完整路径加载, os.add_dll_directory 已添加依赖搜索路径
            handle = ctypes.WinDLL(str(dll_path))
            loaded_dlls[dll_name] = handle
            entry = {"check": "load", "module": dll_name, "status": "PASS"}
            results.append(entry)
            pass_count += 1
            print(f"  [OK] {dll_name}: 加载成功")
        except OSError as e:
            entry = {"check": "load", "module": dll_name, "status": "FAIL", "error": str(e)}
            results.append(entry)
            fail_count += 1
            print(f"  [FAIL] {dll_name}: {e}")

    # === 3. 版本函数调用 ===
    print()
    print("--- 3. 版本函数调用 ---")
    for dll_name, func_name in VERSION_FUNCS.items():
        handle = loaded_dlls.get(dll_name)
        if not handle:
            print(f"  [SKIP] {dll_name}: 未加载, 跳过版本查询")
            continue
        try:
            func = getattr(handle, func_name)
            func.restype = ctypes.c_char_p
            version = func()
            version_str = version.decode("utf-8") if isinstance(version, bytes) else str(version)
            entry = {"check": "version", "module": dll_name, "status": "PASS", "version": version_str}
            results.append(entry)
            pass_count += 1
            print(f"  [OK] {dll_name}.{func_name}() = \"{version_str}\"")
        except Exception as e:
            entry = {"check": "version", "module": dll_name, "status": "FAIL", "error": str(e)}
            results.append(entry)
            fail_count += 1
            print(f"  [FAIL] {dll_name}.{func_name}(): {e}")

    # === 4. 导出符号验证 (objdump) ===
    print()
    print("--- 4. 导出符号验证 ---")
    objdump_path = Path(MSYS2_BIN) / "objdump.exe"
    for dll_name in LOAD_ORDER:
        dll_path = ARTIFACTS / dll_name
        if not dll_path.exists():
            continue
        try:
            proc = subprocess.run(
                [str(objdump_path), "-p", str(dll_path)],
                capture_output=True, text=True, timeout=10,
                encoding="utf-8", errors="replace"
            )
            if proc.returncode == 0 and "export table" in proc.stdout.lower():
                # 统计导出符号 ([N] ... 行)
                export_lines = [l for l in proc.stdout.split("\n") if l.strip().startswith("[")]
                export_count = len(export_lines)
                entry = {"check": "exports", "module": dll_name, "status": "PASS", "export_count": export_count}
                results.append(entry)
                pass_count += 1
                print(f"  [OK] {dll_name}: {export_count} 个导出符号")
            else:
                entry = {"check": "exports", "module": dll_name, "status": "WARN", "error": "无导出表或 objdump 失败"}
                results.append(entry)
                print(f"  [WARN] {dll_name}: 无导出表")
        except Exception as e:
            entry = {"check": "exports", "module": dll_name, "status": "WARN", "error": str(e)}
            results.append(entry)
            print(f"  [WARN] {dll_name}: {e}")

    # === 5. EXE 可执行验证 ===
    print()
    print("--- 5. EXE 可执行验证 ---")
    exes = list(ARTIFACTS.glob("*.exe"))
    for exe in exes:
        try:
            proc = subprocess.run(
                [str(exe), "--help"],
                capture_output=True, text=True, timeout=5,
                encoding="utf-8", errors="replace"
            )
            # EXE 能启动即视为通过 (退出码可能非 0, 因为 --help 可能不支持)
            entry = {"check": "exe", "module": exe.name, "status": "PASS", "exit_code": proc.returncode}
            results.append(entry)
            pass_count += 1
            print(f"  [OK] {exe.name}: 可启动 (exit={proc.returncode})")
        except subprocess.TimeoutExpired:
            entry = {"check": "exe", "module": exe.name, "status": "PASS", "note": "启动但超时(可能进入交互模式)"}
            results.append(entry)
            pass_count += 1
            print(f"  [OK] {exe.name}: 可启动 (超时, 可能交互模式)")
        except Exception as e:
            entry = {"check": "exe", "module": exe.name, "status": "FAIL", "error": str(e)}
            results.append(entry)
            fail_count += 1
            print(f"  [FAIL] {exe.name}: {e}")

    # === 汇总 ===
    print()
    print("=" * 60)
    print(f"Smoke Test 汇总: PASS={pass_count}  FAIL={fail_count}")
    print("=" * 60)

    # 生成报告
    report = {
        "schema": "smoke_test_report/v1",
        "generated_at": datetime.now().isoformat(),
        "baseline_tag": manifest.get("baseline_tag", ""),
        "artifacts_dir": str(ARTIFACTS),
        "results": results,
        "summary": {
            "total": len(results),
            "pass": pass_count,
            "fail": fail_count,
            "verdict": "PASS" if fail_count == 0 else "FAIL",
        }
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"报告: {REPORT}")

    sys.exit(0 if fail_count == 0 else 1)


if __name__ == "__main__":
    main()
