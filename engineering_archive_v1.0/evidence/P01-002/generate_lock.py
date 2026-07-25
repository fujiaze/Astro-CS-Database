"""
P01-002: 生成 dependencies.lock.json
整合 P00-005 环境基线 + P01-002 模块构建配置 + P00-004 依赖图,
生成统一的依赖锁定清单。
"""
import json
import hashlib
from pathlib import Path
from datetime import datetime, timezone

REPO = Path(r"f:\Astro dev\Astro CS Normalization Database")
ENV_BASELINE = REPO / "engineering" / "evidence" / "P00-005" / "environment_baseline.json"
MODULE_CONFIGS = REPO / "engineering" / "evidence" / "P01-002" / "module_build_configs.json"
DEP_GRAPH = REPO / "engineering" / "evidence" / "P00-004" / "dependency_graph.json"
OUT_JSON = REPO / "engineering" / "evidence" / "P01-002" / "dependencies.lock.json"
OUT_MD = REPO / "engineering" / "evidence" / "P01-002" / "dependencies.lock.md"


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    env = json.loads(ENV_BASELINE.read_text(encoding="utf-8"))
    mods = json.loads(MODULE_CONFIGS.read_text(encoding="utf-8"))
    dep = json.loads(DEP_GRAPH.read_text(encoding="utf-8"))

    # 工具链锁定
    toolchain = []
    for t in env.get("toolchain", []):
        toolchain.append({
            "name": t["name"],
            "version": t["version"],
            "path": t["path"],
            "license": t.get("license", ""),
            "sha256": t.get("sha256", ""),
            "locked": True,
        })

    # 外部库锁定（从工具链中提取库类）
    external_libs = []
    lib_names = {
        "GSL (GNU Scientific Library)": "libgsl-28.dll",
        "GSL CBLAS": "libgslcblas-0.dll",
        "zstd": "libzstd.dll",
        "lz4": "liblz4.dll",
        "zlib": "zlib1.dll",
        "OpenMP (libgomp)": "libgomp-1.dll",
    }
    for t in env.get("toolchain", []):
        for lib_name, dll in lib_names.items():
            if t["name"] == lib_name:
                external_libs.append({
                    "name": lib_name,
                    "version": t["version"],
                    "dll": dll,
                    "path": t["path"],
                    "sha256": t.get("sha256", ""),
                    "used_by": [],
                })

    # 标注外部库被哪些模块使用（从模块配置提取）
    lib_usage = {
        "GSL (GNU Scientific Library)": ["star_detector"],
        "GSL CBLAS": ["star_detector"],
        "zstd": ["astro_image_io"],
        "lz4": ["astro_image_io"],
        "zlib": ["gaia_xpsd_client", "healpix_stack"],
        "OpenMP (libgomp)": ["calibration", "dynamic_psf", "gaia_xpsd_client", "star_detector",
                             "snr_estimator", "photometric_calib", "plate_solve_ipv",
                             "healpix_drizzle", "astro_image_io"],
    }
    for lib in external_libs:
        lib["used_by"] = lib_usage.get(lib["name"], [])

    # 补充 Eigen3 和 Qt6（头文件库/特殊）
    for t in env.get("toolchain", []):
        if t["name"] == "Eigen3":
            external_libs.append({
                "name": "Eigen3",
                "version": t["version"],
                "dll": None,
                "path": t["path"],
                "sha256": "",
                "used_by": ["healpix_stack"],
                "type": "header-only",
            })
        elif t["name"] == "Qt6":
            external_libs.append({
                "name": "Qt6",
                "version": t["version"],
                "dll": None,
                "path": t["path"],
                "sha256": t.get("sha256", ""),
                "used_by": ["healpix_browser_qt"],
                "type": "framework",
                "components": ["Core", "Gui", "Widgets", "OpenGLWidgets"],
            })

    # 模块锁定
    modules_locked = []
    for m in mods["modules"]:
        # 确定权威构建系统
        bs = m.get("build_system", "unknown")
        build_ps1 = m.get("build_ps1", {})
        makefile = m.get("makefile", {})
        cmake = m.get("cmake", {})

        # 权威配置优先级: build.ps1 > cmake > makefile
        if build_ps1:
            authority = "build.ps1"
            sources = build_ps1.get("sources", [])
            cflags = build_ps1.get("cflags", [])
            ldlibs = build_ps1.get("ldlibs", [])
            output = build_ps1.get("output", "")
            includes = build_ps1.get("includes", [])
            defines = build_ps1.get("defines", [])
        elif cmake:
            authority = "cmake"
            sources = cmake.get("sources", [])
            cflags = cmake.get("cflags", [])
            ldlibs = cmake.get("ldlibs", [])
            output = cmake.get("output", "")
            includes = cmake.get("includes", [])
            defines = cmake.get("defines", [])
        elif makefile:
            authority = "makefile"
            sources = makefile.get("sources", [])
            cflags = makefile.get("cflags", [])
            ldlibs = makefile.get("ldlibs", [])
            output = makefile.get("output", "")
            includes = makefile.get("includes", [])
            defines = makefile.get("defines", [])
        else:
            authority = "none"
            sources = []
            cflags = []
            ldlibs = []
            output = ""
            includes = []
            defines = []

        modules_locked.append({
            "name": m["module"],
            "path": m["path"],
            "build_system": bs,
            "authority": authority,
            "sources": sources,
            "cflags": cflags,
            "ldlibs": ldlibs,
            "output": output,
            "includes": includes,
            "defines": defines,
            "notes": m.get("notes", ""),
            "makefile_outdated": "makefile" in m and m.get("source_diff", {}).get("build_ps1_only"),
        })

    # 构建顺序（按依赖图分层）
    build_order = {
        "layer_1_base": [
            "astro_image_io", "calibration", "dynamic_psf", "gaia_xpsd_client",
            "star_detector", "snr_estimator"
        ],
        "layer_2_middle": [
            "healpix_drizzle", "healpix_stack", "photometric_calib", "healpix_browser_qt"
        ],
        "layer_3_top": [
            "orchestrator", "plate_solve_ipv"
        ],
        "no_build": ["data_pipeline"],
        "note": "按 P00-004 依赖图分层: 基础层(无跨模块依赖) -> 中间层(依赖基础层) -> 顶层(运行时动态加载)"
    }

    lock = {
        "schema": "dependencies.lock/v1",
        "project": "AstroCS",
        "task_id": "P01-002",
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "baseline_tag": "astrocs-baseline-p00",
        "toolchain": toolchain,
        "external_libs": external_libs,
        "modules": modules_locked,
        "build_order": build_order,
        "path_requirements": {
            "msys2_mingw64_bin": "C:\\msys64\\mingw64\\bin",
            "must_be_in_path": True,
            "note": "GCC/mingw32-make/qmake6 不在默认 PATH, 根级 build.ps1 需注入此路径",
        },
        "lock_integrity": {
            "environment_baseline_sha256": sha256_file(ENV_BASELINE),
            "module_build_configs_sha256": sha256_file(MODULE_CONFIGS),
            "dependency_graph_sha256": sha256_file(DEP_GRAPH),
        }
    }

    OUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUT_JSON.write_text(json.dumps(lock, ensure_ascii=False, indent=2), encoding="utf-8")

    # 生成 MD 摘要
    md_lines = []
    md_lines.append("# AstroCS 依赖锁定清单 — P01-002\n")
    md_lines.append(f"- **生成时间**: {lock['generated_at']}\n")
    md_lines.append(f"- **基线 Tag**: `{lock['baseline_tag']}`\n")
    md_lines.append(f"- **Schema**: `{lock['schema']}`\n\n")

    md_lines.append("## 1. 工具链锁定\n\n")
    md_lines.append("| # | 工具 | 版本 | 路径 | SHA-256 |\n|---|---|---|---|---|\n")
    for i, t in enumerate(toolchain, 1):
        sha = t["sha256"][:16] + "..." if t["sha256"] else "—"
        md_lines.append(f"| {i} | {t['name']} | {t['version']} | `{t['path']}` | {sha} |\n")

    md_lines.append("\n## 2. 外部库锁定\n\n")
    md_lines.append("| 库 | 版本 | DLL | 使用模块 |\n|---|---|---|---|\n")
    for lib in external_libs:
        dll = lib["dll"] or "(header-only/framework)"
        used = ", ".join(lib["used_by"]) if lib["used_by"] else "—"
        md_lines.append(f"| {lib['name']} | {lib['version']} | {dll} | {used} |\n")

    md_lines.append("\n## 3. 模块构建锁定\n\n")
    md_lines.append("| 模块 | 权威构建 | 输出 | 源文件数 | Makefile 过时 |\n|---|---|---|---|---|\n")
    for m in modules_locked:
        outdated = "⚠ 是" if m["makefile_outdated"] else "否"
        md_lines.append(f"| {m['name']} | {m['authority']} | {m['output'] or '—'} | {len(m['sources'])} | {outdated} |\n")

    md_lines.append("\n## 4. 构建顺序\n\n")
    for layer, modules in build_order.items():
        if layer == "note":
            continue
        md_lines.append(f"- **{layer}**: {', '.join(modules)}\n")
    md_lines.append(f"\n> {build_order['note']}\n")

    md_lines.append("\n## 5. 路径要求\n\n")
    md_lines.append(f"- MSYS2 MinGW64 bin: `{lock['path_requirements']['msys2_mingw64_bin']}`\n")
    md_lines.append(f"- 必须在 PATH: {lock['path_requirements']['must_be_in_path']}\n")
    md_lines.append(f"> {lock['path_requirements']['note']}\n")

    md_lines.append("\n## 6. 锁定完整性\n\n")
    md_lines.append("| 来源文件 | SHA-256 |\n|---|---|\n")
    for name, sha in lock["lock_integrity"].items():
        md_lines.append(f"| {name} | {sha[:32]}... |\n")

    OUT_MD.write_text("".join(md_lines), encoding="utf-8")

    print(f"OK: dependencies.lock.json generated")
    print(f"  toolchain: {len(toolchain)} tools")
    print(f"  external_libs: {len(external_libs)} libs")
    print(f"  modules: {len(modules_locked)} modules")
    print(f"  build_order: {sum(len(v) for k,v in build_order.items() if k!='note')} modules in 3 layers + 1 no_build")
    print(f"  output: {OUT_JSON}")


if __name__ == "__main__":
    main()
