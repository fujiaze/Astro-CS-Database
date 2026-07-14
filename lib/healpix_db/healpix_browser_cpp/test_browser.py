# -*- coding: utf-8 -*-
"""
test_browser.py - 浏览器文件打开支持测试
功能: 验证 browser_cpp.exe 能正确打开 .hiss 和 .hcsd 文件
用途: Task 9 文件打开支持验证

测试流程:
  1. 用 healpix_io 创建测试 .hiss 文件 (10 个像素, nside=64)
  2. 用 healpix_io 创建测试 .hcsd 文件 (10 个像素, nside=64)
  3. 启动 browser_cpp.exe 测试 .hiss 文件
  4. 用 requests 访问 http://localhost:18080/api/file_info 验证返回 is_hiss=true
  5. 停止 browser_cpp.exe
  6. 启动 browser_cpp.exe 测试 .hcsd 文件
  7. 用 requests 访问 http://localhost:18080/api/file_info 验证返回 is_hiss=false
  8. 停止 browser_cpp.exe

验证标准:
  - .hiss 文件打开成功, file_info 返回 is_hiss=true, nside=64, n_pix=10
  - .hcsd 文件打开成功, file_info 返回 is_hiss=false, nside=64, n_pix=10

用法:
  cd "f:\\Astro dev\\Astro CS Normalization Database"
  python lib\\healpix_db\\healpix_browser_cpp\\test_browser.py
"""

from __future__ import annotations

import logging
import os
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
import requests

# 将 healpix_io 模块路径加入 sys.path
THIS_DIR = Path(__file__).resolve().parent
HEALPIX_IO_DIR = THIS_DIR.parent / "healpix_io"
sys.path.insert(0, str(HEALPIX_IO_DIR))

from healpix_io import HissWriter, HcsdWriter  # noqa: E402

# ============================================================================
# 配置
# ============================================================================

logging.basicConfig(
    level=logging.INFO,
    format="[%(levelname)s] %(message)s",
)
logger = logging.getLogger("test_browser")

# browser_cpp.exe 路径
BROWSER_EXE = THIS_DIR / "browser_cpp.exe"
# 测试文件路径
TEST_HISS = THIS_DIR / "test_data.hiss"
TEST_HCSD = THIS_DIR / "test_data.hcsd"
# HTTP 服务器地址
API_BASE = "http://localhost:18080"
# 测试像素数
N_PIX = 10
# 测试 nside (nside=64 时, 每个像素就是一个子叶)
TEST_NSIDE = 64

# 启动等待时间 (秒)
STARTUP_WAIT_SEC = 2.0
# 请求超时 (秒)
REQUEST_TIMEOUT = 10


# ============================================================================
# 测试数据创建
# ============================================================================

def create_test_hiss(path: Path) -> None:
    """创建测试 .hiss 文件 (10 个像素, nside=64)

    Args:
        path: 输出文件路径
    """
    logger.info("创建测试 .hiss 文件: %s", path)

    # 10 个像素, ipix 从 0 到 9 (nside=64 时每个像素就是一个子叶)
    ipix = np.arange(N_PIX, dtype=np.uint64)
    pixel = np.linspace(1.0, 10.0, N_PIX, dtype=np.float32)
    meta = {
        "filter": "Lum",
        "exposure": 60.0,
        "test": "hiss_test",
    }

    writer = HissWriter(str(path), TEST_NSIDE, nested=True)
    ret = writer.write(ipix, pixel, meta)
    if ret != 0:
        raise RuntimeError(f"hiss_write 失败, 返回码={ret}")

    logger.info("  ipix=%s", ipix.tolist())
    logger.info("  pixel=%s", pixel.tolist())
    logger.info("  nside=%d, nested=True, n_pix=%d", TEST_NSIDE, N_PIX)


def create_test_hcsd(path: Path) -> None:
    """创建测试 .hcsd 文件 (10 个像素, nside=64)

    Args:
        path: 输出文件路径
    """
    logger.info("创建测试 .hcsd 文件: %s", path)

    # 10 个像素, ipix 从 0 到 9 (nside=64 时每个像素就是一个子叶)
    ipix = np.arange(N_PIX, dtype=np.uint64)
    pixel = np.linspace(1.0, 10.0, N_PIX, dtype=np.float32)
    meta = {
        "filter": "Lum",
        "exposure": 60.0,
        "test": "hcsd_test",
    }

    writer = HcsdWriter(str(path), TEST_NSIDE, nested=True)
    ret = writer.write(ipix, pixel, meta)
    if ret != 0:
        raise RuntimeError(f"hcsd_write 失败, 返回码={ret}")

    logger.info("  ipix=%s", ipix.tolist())
    logger.info("  pixel=%s", pixel.tolist())
    logger.info("  nside=%d, nested=True, n_pix=%d", TEST_NSIDE, N_PIX)


# ============================================================================
# browser_cpp.exe 进程管理
# ============================================================================

def start_browser(file_path: Path) -> subprocess.Popen:
    """启动 browser_cpp.exe 打开指定文件

    Args:
        file_path: 要打开的 .hiss 或 .hcsd 文件路径

    Returns:
        subprocess.Popen 进程对象
    """
    if not BROWSER_EXE.is_file():
        raise FileNotFoundError(f"browser_cpp.exe 不存在: {BROWSER_EXE}")

    logger.info("启动 browser_cpp.exe: %s %s", BROWSER_EXE, file_path)

    # 启动子进程 (不创建新窗口, 重定向 stdout/stderr)
    creationflags = 0
    if os.name == "nt":
        # CREATE_NO_WINDOW = 0x08000000 (避免弹出控制台窗口)
        creationflags = subprocess.CREATE_NO_WINDOW

    proc = subprocess.Popen(
        [str(BROWSER_EXE), str(file_path)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        creationflags=creationflags,
    )

    # 等待 HTTP 服务器启动
    logger.info("等待 HTTP 服务器启动 (%.1fs)...", STARTUP_WAIT_SEC)
    time.sleep(STARTUP_WAIT_SEC)

    # 检查进程是否仍在运行
    if proc.poll() is not None:
        stdout, stderr = proc.communicate()
        logger.error("browser_cpp.exe 启动后立即退出, 返回码=%d", proc.returncode)
        logger.error("stdout: %s", stdout.decode("utf-8", errors="replace"))
        logger.error("stderr: %s", stderr.decode("utf-8", errors="replace"))
        raise RuntimeError(f"browser_cpp.exe 启动失败, 返回码={proc.returncode}")

    logger.info("browser_cpp.exe 已启动, PID=%d", proc.pid)
    return proc


def stop_browser(proc: subprocess.Popen) -> None:
    """停止 browser_cpp.exe 进程

    Args:
        proc: subprocess.Popen 进程对象
    """
    if proc.poll() is not None:
        logger.info("browser_cpp.exe 已退出, 返回码=%d", proc.returncode)
        # 读取剩余输出
        stdout, stderr = proc.communicate()
        if stdout:
            logger.info("stdout: %s", stdout.decode("utf-8", errors="replace"))
        if stderr:
            logger.info("stderr: %s", stderr.decode("utf-8", errors="replace"))
        return

    logger.info("停止 browser_cpp.exe (PID=%d)...", proc.pid)
    proc.terminate()
    try:
        proc.wait(timeout=5)
        logger.info("browser_cpp.exe 已停止, 返回码=%d", proc.returncode)
    except subprocess.TimeoutExpired:
        logger.warning("browser_cpp.exe 未响应 terminate, 强制 kill")
        proc.kill()
        proc.wait(timeout=3)

    # 读取输出 (用于调试)
    try:
        stdout, stderr = proc.communicate(timeout=1)
        if stdout:
            for line in stdout.decode("utf-8", errors="replace").splitlines():
                logger.info("  [exe stdout] %s", line)
        if stderr:
            for line in stderr.decode("utf-8", errors="replace").splitlines():
                logger.info("  [exe stderr] %s", line)
    except Exception:
        pass


# ============================================================================
# API 验证
# ============================================================================

def get_file_info() -> dict:
    """访问 /api/file_info 获取文件信息

    Returns:
        dict: 文件信息 JSON
    """
    url = f"{API_BASE}/api/file_info"
    logger.info("GET %s", url)
    resp = requests.get(url, timeout=REQUEST_TIMEOUT)
    resp.raise_for_status()
    return resp.json()


def verify_file_info(info: dict, expected_is_hiss: bool,
                     expected_nside: int, expected_n_pix: int) -> bool:
    """验证 file_info 返回值是否符合预期

    Args:
        info: /api/file_info 返回的 JSON
        expected_is_hiss: 期望的 is_hiss 值
        expected_nside: 期望的 nside 值
        expected_n_pix: 期望的 n_pix 值

    Returns:
        bool: 验证是否通过
    """
    ok = True
    logger.info("file_info 返回: %s", info)

    # 检查 is_hiss
    actual_is_hiss = info.get("is_hiss")
    if actual_is_hiss != expected_is_hiss:
        logger.error("  [FAIL] is_hiss: 期望=%s, 实际=%s",
                     expected_is_hiss, actual_is_hiss)
        ok = False
    else:
        logger.info("  [OK] is_hiss=%s", actual_is_hiss)

    # 检查 is_hcsd (应与 is_hiss 互补)
    actual_is_hcsd = info.get("is_hcsd")
    expected_is_hcsd = not expected_is_hiss
    if actual_is_hcsd != expected_is_hcsd:
        logger.error("  [FAIL] is_hcsd: 期望=%s, 实际=%s",
                     expected_is_hcsd, actual_is_hcsd)
        ok = False
    else:
        logger.info("  [OK] is_hcsd=%s", actual_is_hcsd)

    # 检查 nside
    actual_nside = info.get("nside")
    if actual_nside != expected_nside:
        logger.error("  [FAIL] nside: 期望=%d, 实际=%s",
                     expected_nside, actual_nside)
        ok = False
    else:
        logger.info("  [OK] nside=%s", actual_nside)

    # 检查 n_pix
    actual_n_pix = info.get("n_pix")
    if actual_n_pix != expected_n_pix:
        logger.error("  [FAIL] n_pix: 期望=%d, 实际=%s",
                     expected_n_pix, actual_n_pix)
        ok = False
    else:
        logger.info("  [OK] n_pix=%s", actual_n_pix)

    # 检查 file_path 存在
    file_path = info.get("file_path")
    if not file_path:
        logger.error("  [FAIL] file_path 为空")
        ok = False
    else:
        logger.info("  [OK] file_path=%s", file_path)

    return ok


def verify_all_data() -> bool:
    """访问 /api/all_data 验证全量数据 (.hiss 模式)

    Returns:
        bool: 验证是否通过
    """
    url = f"{API_BASE}/api/all_data"
    logger.info("GET %s", url)
    try:
        resp = requests.get(url, timeout=REQUEST_TIMEOUT)
        if resp.status_code != 200:
            logger.error("  [FAIL] HTTP %d: %s", resp.status_code, resp.text[:200])
            return False
        data = resp.json()
        logger.info("  all_data: nside=%s, n_pix=%s", data.get("nside"), data.get("n_pix"))
        if data.get("n_pix") != N_PIX:
            logger.error("  [FAIL] n_pix: 期望=%d, 实际=%s", N_PIX, data.get("n_pix"))
            return False
        logger.info("  [OK] all_data 返回 %d 个像素", data.get("n_pix"))
        return True
    except Exception as e:
        logger.error("  [FAIL] all_data 请求失败: %s", e)
        return False


# ============================================================================
# 测试用例
# ============================================================================

def test_hiss_file() -> bool:
    """测试打开 .hiss 文件 (单帧模式)

    Returns:
        bool: 测试是否通过
    """
    print()
    print("=" * 60)
    print("测试 1: 打开 .hiss 文件 (单帧模式)")
    print("=" * 60)

    # 1) 创建测试文件
    create_test_hiss(TEST_HISS)

    # 2) 启动 browser_cpp.exe
    proc = None
    try:
        proc = start_browser(TEST_HISS)

        # 3) 验证 /api/file_info
        info = get_file_info()
        ok = verify_file_info(info, expected_is_hiss=True,
                              expected_nside=TEST_NSIDE, expected_n_pix=N_PIX)

        # 4) 验证 /api/all_data (.hiss 模式应能返回全量数据)
        ok_all = verify_all_data()
        ok = ok and ok_all

        if ok:
            print("\n[结果] .hiss 文件测试: PASS")
        else:
            print("\n[结果] .hiss 文件测试: FAIL")
        return ok

    finally:
        # 5) 停止 browser_cpp.exe
        if proc is not None:
            stop_browser(proc)


def test_hcsd_file() -> bool:
    """测试打开 .hcsd 文件 (球面模式)

    Returns:
        bool: 测试是否通过
    """
    print()
    print("=" * 60)
    print("测试 2: 打开 .hcsd 文件 (球面模式)")
    print("=" * 60)

    # 1) 创建测试文件
    create_test_hcsd(TEST_HCSD)

    # 2) 启动 browser_cpp.exe
    proc = None
    try:
        proc = start_browser(TEST_HCSD)

        # 3) 验证 /api/file_info
        info = get_file_info()
        ok = verify_file_info(info, expected_is_hiss=False,
                              expected_nside=TEST_NSIDE, expected_n_pix=N_PIX)

        # 4) .hcsd 模式下 /api/all_data 应返回 400 (not in hiss mode)
        url = f"{API_BASE}/api/all_data"
        logger.info("GET %s (应返回 400, 因为是 .hcsd 模式)", url)
        try:
            resp = requests.get(url, timeout=REQUEST_TIMEOUT)
            if resp.status_code == 400:
                logger.info("  [OK] all_data 在 .hcsd 模式下返回 400 (符合预期)")
            else:
                logger.warning("  [WARN] all_data 在 .hcsd 模式下返回 %d (期望 400)",
                               resp.status_code)
        except Exception as e:
            logger.warning("  [WARN] all_data 请求异常: %s", e)

        if ok:
            print("\n[结果] .hcsd 文件测试: PASS")
        else:
            print("\n[结果] .hcsd 文件测试: FAIL")
        return ok

    finally:
        # 5) 停止 browser_cpp.exe
        if proc is not None:
            stop_browser(proc)


def test_nonexistent_file() -> bool:
    """测试打开不存在的文件 (错误处理)

    Returns:
        bool: 测试是否通过
    """
    print()
    print("=" * 60)
    print("测试 3: 打开不存在的文件 (错误处理)")
    print("=" * 60)

    nonexistent = THIS_DIR / "nonexistent_file.hiss"
    logger.info("尝试打开不存在的文件: %s", nonexistent)

    if not BROWSER_EXE.is_file():
        raise FileNotFoundError(f"browser_cpp.exe 不存在: {BROWSER_EXE}")

    creationflags = 0
    if os.name == "nt":
        creationflags = subprocess.CREATE_NO_WINDOW

    proc = subprocess.Popen(
        [str(BROWSER_EXE), str(nonexistent)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        creationflags=creationflags,
    )

    # 等待进程退出 (应立即退出, 返回码=1)
    try:
        stdout, stderr = proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        stdout, stderr = proc.communicate()
        logger.error("browser_cpp.exe 未在 5s 内退出 (期望立即退出)")
        return False

    ret_code = proc.returncode
    stderr_text = stderr.decode("utf-8", errors="replace")
    logger.info("返回码=%d", ret_code)
    logger.info("stderr: %s", stderr_text)

    if ret_code != 0 and "文件不存在" in stderr_text:
        logger.info("[OK] 不存在的文件错误处理正确 (返回码=%d, 输出友好错误信息)", ret_code)
        print("\n[结果] 不存在文件错误处理测试: PASS")
        return True
    else:
        logger.error("[FAIL] 错误处理不正确")
        print("\n[结果] 不存在文件错误处理测试: FAIL")
        return False


# ============================================================================
# 清理
# ============================================================================

def cleanup() -> None:
    """清理测试文件"""
    for f in [TEST_HISS, TEST_HCSD]:
        if f.exists():
            try:
                f.unlink()
                logger.info("已删除测试文件: %s", f)
            except Exception as e:
                logger.warning("删除测试文件失败: %s: %s", f, e)


# ============================================================================
# 主入口
# ============================================================================

def main() -> int:
    print("=" * 60)
    print("test_browser.py - 浏览器文件打开支持测试 (Task 9)")
    print("=" * 60)
    print(f"browser_cpp.exe: {BROWSER_EXE}")
    print(f"测试参数: nside={TEST_NSIDE}, n_pix={N_PIX}")
    print(f"API 基址: {API_BASE}")

    # 前置检查
    if not BROWSER_EXE.is_file():
        print(f"\n[错误] browser_cpp.exe 不存在: {BROWSER_EXE}")
        print("请先运行 build.ps1 编译")
        return 1

    # 检查 requests 模块
    try:
        import requests  # noqa: F401
    except ImportError:
        print("\n[错误] 缺少 requests 模块, 请运行: pip install requests")
        return 1

    # 检查 numpy 模块
    try:
        import numpy  # noqa: F401
    except ImportError:
        print("\n[错误] 缺少 numpy 模块, 请运行: pip install numpy")
        return 1

    results = []
    try:
        # 测试 1: .hiss 文件
        results.append(("hiss_file", test_hiss_file()))

        # 测试 2: .hcsd 文件
        results.append(("hcsd_file", test_hcsd_file()))

        # 测试 3: 不存在的文件 (错误处理)
        results.append(("nonexistent_file", test_nonexistent_file()))

    finally:
        # 清理测试文件
        print()
        print("=" * 60)
        print("清理测试文件")
        print("=" * 60)
        cleanup()

    # 汇总结果
    print()
    print("=" * 60)
    print("测试汇总")
    print("=" * 60)
    all_pass = True
    for name, ok in results:
        status = "PASS" if ok else "FAIL"
        print(f"  {name}: {status}")
        if not ok:
            all_pass = False

    print()
    if all_pass:
        print("=" * 60)
        print("所有测试通过!")
        print("=" * 60)
        return 0
    else:
        print("=" * 60)
        print("部分测试失败!")
        print("=" * 60)
        return 1


if __name__ == "__main__":
    sys.exit(main())
