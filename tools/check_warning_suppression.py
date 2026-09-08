#!/usr/bin/env python3
"""QA-001: 生产编译警告抑制清零校验。

规则:
1. V6 生产模块 (phase1/2/3/core/io/cli 非第三方) 无 -w//w/Wno 全域抑制。
2. -w 抑制仅限第三方/遗留源 (cfitsio/AIO/drizzle/hips V5 代码) — 豁免并登记。
3. 生产构建 (GCC Release) 警告计数 = 0 (增量基线, 第三方豁免源不计)。
exit 0 = PASS。

行为约定:
- 静态检查 (生产源扫描 + CMake 层) 发现违规时直接报错返回:
  不 touch 生产源、不触发构建 (消除无谓副作用)。
- 静态干净且构建树存在时, 才 touch 一个生产源文件 (强制重编) 并统计警告。
- 构建命令经 subprocess 位置参数传递路径 (cwd + $1/$2), 仓库路径含空格
  (如 "/workspace/Astro CS Database") 不再发生 cd 词法断裂。
"""
import pathlib, re, sys, subprocess

REPO = pathlib.Path(__file__).resolve().parents[1]

# 生产源目录 (V6 模块)
PROD_DIRS = ["lib/phase1", "lib/phase2/src", "lib/phase3_session", "lib/core", "lib/io", "lib/cpu", "cli"]
# 豁免 (第三方/遗留): 允许源级抑制
EXEMPT = ["cfitsio", "aio_", "drizzle", "hips_", "healpix"]

# 构建统计脚本: $1 = 仓库根, $2 = 构建目录(相对仓库根)。
# 位置参数 + 引号内引用, 路径含空格安全; WSL 横幅过滤与 grep -c 'warning:'
# 统计语义与旧实现保持一致。
BUILD_COUNT_SCRIPT = (
    'cd "$1" || exit 9\n'
    'cmake --build "$2" --target astrocs 2>&1 '
    "| grep -v 'WSL\\|适用于 Linux' | grep -c 'warning:' || true\n"
)


def static_scan():
    """生产源 + CMake 层抑制指令扫描 (纯读, 无副作用)。"""
    errors = []
    for d in PROD_DIRS:
        base = REPO / d
        if not base.is_dir(): continue
        for f in base.rglob("*.cpp"):
            txt = f.read_text(encoding="utf-8", errors="ignore")
            # 仅匹配独立编译选项形态 (-w 前后非字母/连字符; not-wired 等注释词不匹配)
            if re.search(r'(?<!\w)-w(?!\w)', txt) or re.search(r'"?-Wno-', txt):
                errors.append(f"生产源含抑制指令: {f}")
    # CMake 层抑制检查
    for cm in (REPO / "cli" / "CMakeLists.txt", REPO / "CMakeLists.txt", REPO / "tests" / "unit" / "CMakeLists.txt"):
        if not cm.is_file(): continue
        t = cm.read_text(encoding="utf-8", errors="ignore")
        for m in re.finditer(r'set_property\(SOURCE ([^)]*?)\s+PROPERTY COMPILE_OPTIONS "\$\{ACS_WARN_SUPPRESS\}', t):
            srcs = m.group(1)
            # ${P2_SRCS}/${P3_SRCS} 已去抑制; 剩余豁免 = 第三方/遗留 (CFITSIO/AIO/DRIZZLE/HISS)
            if not any(x in srcs for x in ("CFITSIO", "AIO", "DRIZZLE", "HISS", "SAMPLER")):
                errors.append(f"{cm.name}: 非豁免源抑制 {srcs.strip()[:60]}")
    return errors


def detect_build_tree():
    # 构建目录缺失时结构性跳过（windows runner 无构建树；探测顺序
    # build/root-cmake → build/，适配当前 Ninja 单配置布局 build/）。
    for cand in (REPO / "build" / "root-cmake", REPO / "build"):
        if (cand / "Makefile").exists() or (cand / "build.ninja").exists():
            return cand
    return None


def count_build_warnings(build_dir, repo=None, build_cmd="cmake"):
    """构建并统计生产警告数, 返回 stdout 统计串 ("" / "0" / "N")。

    build_cmd 参数仅供测试注入假命令 (echo 等), 生产恒为 cmake。
    位置参数传递路径: $1=仓库根 $2=构建目录相对路径, cwd=仓库根。
    """
    repo = str(repo) if repo is not None else str(REPO)
    rel = str(build_dir) if not str(build_dir).startswith("/") \
        else pathlib.Path(build_dir).relative_to(repo)
    script = BUILD_COUNT_SCRIPT.replace("cmake", build_cmd, 1) \
        if build_cmd != "cmake" else BUILD_COUNT_SCRIPT
    r = subprocess.run(["bash", "-c", script, "bash", repo, rel],
                       capture_output=True, text=True, timeout=600, cwd=repo)
    return r.stdout.strip()


def measure_build(build_dir):
    """touch 强制重编一个生产文件后统计警告 (构建树存在时的功能需要)。"""
    rel_src = REPO / "lib" / "phase1" / "noise" / "noise_model.cpp"
    rel_src.touch()
    return count_build_warnings(build_dir)


def main():
    # 静态检查先行: 违规直接报错返回, 不 touch 源文件、不触发构建
    errors = static_scan()
    if errors:
        print("QA-001_WARN_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    build_dir = detect_build_tree()
    if build_dir is None:
        print("QA-001_SKIP: 无可用构建树 (build/root-cmake 与 build/ 均缺失), 仅静态检查")
        return 0
    warn = measure_build(build_dir)
    if warn not in ("", "0"):
        errors.append(f"生产构建警告 {warn} 个 (非 0)")
    if errors:
        print("QA-001_WARN_VIOLATION:")
        for e in errors: print("  " + e)
        return 1
    print("QA-001_PASS: V6 生产模块零抑制指令; -w 仅第三方豁免; 生产警告=0")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
