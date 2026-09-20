#!/usr/bin/env python3
"""汇总 λs 扫描指标 → markdown 表 + ASCII 曲线（供 docs/smooth-lambda.md 引用）。"""
import json, os, sys, math
ROOT = "/workspace/Astro CS Database"
D = os.path.join(ROOT, "run/reverse_verify/smooth_lambda")
FENCE = chr(96) * 3
SCENES = [("A_base","合成 A 基础：加性天光 + 突兀亮峰（各帧一致）+ 光子散粒噪声"),
          ("A_vary","合成 A 变体：亮峰**帧间不同**（amp/FWHM 各帧抖动 10%）"),
          ("A_tau","合成 A 变体：额外 2% 逐帧乘性透过率"),
          ("A_nosky","负例 1：真值**无天光**（B_f=0）"),
          ("A_nocore","负例 2：真值**无亮结构**（A_core=0）"),
          ("A_scale","尺度不变性：A_base x 1e9（仅单位标签变化）"),
          ("B_prod","合成 B：生产实测尺度（patch 空间 MAD 18% 天空、逐帧天光差 14%）"),
          ("B_nosky","负例 1'：生产尺度 + 无天光"),
          ("real49","**真实 L4 49 帧**（p2_samples.json 277234 obs / 33472 control）")]

def load(name):
    p = os.path.join(D, "sw_%s.metrics.json" % name)
    if not os.path.exists(p): return None
    try: return json.load(open(p))
    except Exception: return None

def fmt(v, nd=4):
    if v is None: return "—"
    if isinstance(v, float):
        if v != v: return "nan"
        if v == 0: return "0"
        if abs(v) >= 1e5 or abs(v) < 1e-3: return "%.3e" % v
        return ("%%.%df" % nd) % v
    return str(v)

def table(rows, cols, hdr):
    out = ["| " + " | ".join(hdr) + " |", "|" + "|".join(["---"]*len(hdr)) + "|"]
    for r in rows:
        out.append("| " + " | ".join(fmt(r.get(c)) for c in cols) + " |")
    return "\n".join(out)

def ascii_curve(xs, ys, title, ylab="", logx=True, w=54, h=11):
    pts = [(x,y) for x,y in zip(xs,ys) if y is not None and y==y]
    if not pts: return ""
    X = [math.log10(max(x,1e-12)) if logx else x for x,_ in pts]
    Y = [y for _,y in pts]
    x0,x1 = min(X),max(X); y0,y1 = min(Y),max(Y)
    if x1-x0 < 1e-12: x1 = x0+1
    if y1-y0 < 1e-30: y1 = y0+1
    grid = [[" "]*w for _ in range(h)]
    for xx,yy in zip(X,Y):
        cx = int(round((xx-x0)/(x1-x0)*(w-1)))
        cy = h-1-int(round((yy-y0)/(y1-y0)*(h-1)))
        grid[cy][cx] = "*"
    lines = ["%s   (ylab=%s,  min=%s max=%s,  x 轴 log10)" % (title, ylab, fmt(min(Y)), fmt(max(Y)))]
    for i,row in enumerate(grid):
        v = y1 - (y1-y0)*i/(h-1)
        lines.append("  %11s |%s|" % (fmt(v,3), "".join(row)))
    lines.append("              +" + "-"*w + "+")
    lines.append("               " + " ".join("%g" % x for x,_ in pts)[:w+20])
    return "\n".join(lines)

def main():
    out = []
    for name, desc in SCENES:
        rows = load(name)
        if not rows:
            out.append("### %s" % name); out.append(""); out.append("（尚无结果）"); out.append(""); continue
        out.append("### %s — %s" % (name, desc)); out.append("")
        allc = ["lambda","converged","iterations","seconds","c_rms","c_grad_rms",
                "removal_frac","resid_over_noise","resid_over_sky","step_ratio",
                "dim_bright_med","dim_peak_med","dim_faint_med","corr_C_B_med"]
        hmap = {"lambda":"ls","converged":"conv","iterations":"it","seconds":"s",
                "c_rms":"C_rms","c_grad_rms":"C_grad_rms","removal_frac":"天光去除率",
                "resid_over_noise":"残差/噪声","resid_over_sky":"残差/天光","step_ratio":"台阶比",
                "dim_bright_med":"亮区压暗","dim_peak_med":"峰区压暗","dim_faint_med":"暗区压暗",
                "corr_C_B_med":"corr(C,B)"}
        cols = [c for c in allc if any(c in r for r in rows)]
        out.append(table(rows, cols, [hmap[c] for c in cols])); out.append("")
        for yc in ["removal_frac","step_ratio","dim_bright_med","c_grad_rms","resid_over_noise"]:
            xs = [r["lambda"] for r in rows if r.get(yc) is not None]
            ys = [r.get(yc) for r in rows if r.get(yc) is not None]
            if xs: out.append(FENCE); out.append(ascii_curve(xs, ys, name + " " + hmap[yc], ylab=hmap[yc])); out.append(FENCE); out.append("")
    txt = "\n".join(out)
    open(os.path.join(D, "report_tables.md"), "w").write(txt)
    print("wrote", os.path.join(D, "report_tables.md"), len(txt), "bytes")

if __name__ == "__main__":
    main()
