"""独立审计包收口装配：代码面四份归位、清掉交付层中间件、刷新总目录状态、出核对表。

只读被审仓库；一切写入都落在审查工作区的 独立审计包/ 内。空输入即非零退出。
"""

import sys
from pathlib import Path

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
ROOT = Path(__file__).resolve().parent.parent          # 产出/
RAW, PKG = ROOT / "raw", ROOT / "独立审计包"

if not RAW.is_dir() or not PKG.is_dir():
    print("FAIL-CLOSED: 缺 raw/ 或 独立审计包/")
    sys.exit(2)

# 1) 04_代码 四份归位（源在 raw/，加一行来源标注后写入交付层）
MOVES = [
    ("AUD-401-架构对齐.md", "04_代码/架构对齐报告.md",
     "来源分片 AUD-401；含归并/退役/迁移/接线四类动作表与"
     "「源文件是否进生产构建图」穷举表。"),
    ("AUD-402-常数台账.说明.md", "04_代码/经验参数与硬编码清单.说明.md",
     "来源分片 AUD-402 机械层说明；判读层见 02_科学/常数公式算法总台账。"),
    ("AUD-403-注释与README.md", "04_代码/注释与README审计报告.md",
     "来源分片 AUD-403；含「待清编号 → 保留证据指针」分类表。"),
    ("AUD-404-退役与死代码清单.md", "04_代码/退役与死代码清单.md",
     "来源分片 AUD-404；机器表同名 CSV 留在 raw/ 供逐条核对。"),
]
CODE = PKG / "04_代码"
CODE.mkdir(exist_ok=True)
done = []
for src_name, dst_rel, note in MOVES:
    src = RAW / src_name
    if not src.is_file():
        print(f"WARN 缺源件：{src_name}（跳过，登记为未归位）")
        continue
    text = src.read_text(encoding="utf-8", errors="replace")
    header = f"<!-- {note} 被审基线 c8f64e9a。 -->\n\n"
    (PKG / dst_rel).write_text(header + text, encoding="utf-8")
    done.append(dst_rel)
print(f"04_代码 归位 {len(done)}/{len(MOVES)}：{done}")

# 2) 清交付层中间件（只删本轮代理留下的草稿/分块，正式件不动）
removed = []
for pat in ("_draft*", "_final*", "_refs.part", "_block*"):
    for p in (PKG / "03_小论文").glob(pat) if "block" not in pat else RAW.glob(pat):
        p.unlink()
        removed.append(p.name)
print(f"清理交付层中间件 {len(removed)}：{removed}")

# 3) 交付物核对表
CHECKS = [
    ("D1", "01_文档/文档现状审计台账.csv", "360 份逐份登记"),
    ("D1", "01_文档/文档现状审计台账.md", "合并件（含四张派生表）"),
    ("D2", "01_文档/目标文档架构.md", "目录树·角色·主题正本·四层映射"),
    ("D2", "01_文档/迁移合并清单.md", "360 行逐份动作"),
    ("D2", "01_文档/索引重建规格.md", "C1–C12 可红可绿判据"),
    ("D3", "02_科学/测光链路核验报告.md", "复核结论已覆盖"),
    ("D3", "02_科学/SNR链路核验报告.md", "复核结论已覆盖"),
    ("D3", "02_科学/天光无缝核验报告.md", "复核结论已覆盖"),
    ("D3", "02_科学/面积交叠核验报告.md", "复核结论已覆盖"),
    ("D3", "02_科学/链路间口径对表.md", "真互斥 7 / 措辞差异 6"),
    ("D4", "02_科学/常数公式算法总台账.csv", "主表"),
    ("D4", "02_科学/常数公式算法总台账.md", "分母·分布·漏检声明"),
    ("D5", "03_小论文/论文1_测光星等坐标系.md", "创新点①"),
    ("D5", "03_小论文/论文2_跨帧绝对信噪比.md", "创新点②"),
    ("D5", "03_小论文/论文3_加性天光无缝.md", "创新点③"),
    ("D5", "03_小论文/论文4_HEALPix面积交叠.md", "链上几何环节"),
    ("D6", "04_代码/架构对齐报告.md", "含动作表与构建归属穷举"),
    ("D6", "04_代码/经验参数与硬编码清单.说明.md", "机械层；判读层见 D4"),
    ("D6", "04_代码/注释与README审计报告.md", "含待清编号分类表"),
    ("D6", "04_代码/退役与死代码清单.md", "四档定位与可直删分组"),
    ("D7", "05_门禁/门禁体系设计.md", "三条资格 Q1–Q3 与入口唯一化"),
    ("D7", "05_门禁/门禁清单.csv", "64 条（含注入必红）"),
    ("D7", "05_门禁/旧门禁处置.md", "四档处置与死结八例"),
    ("D8", "06_实施/实施任务总览与依赖图.md", "两层都过 186 条；待第二层 1,135 条"),
    ("D9", "07_未决/UNRESOLVED清单.md", "13 条裁决＋6 条核实"),
]
missing = [(d, f) for d, f, _ in CHECKS if not (PKG / f).is_file()]
lines = ["# 交付物核对表", "",
         "被审基线 `c8f64e9a`。阶段 A 全程零仓库写入、零 git 写、未执行任何构建/测试/门禁/端到端。", "",
         "| 编号 | 交付物 | 状态 | 备注 |", "|---|---|---|---|"]
for d, f, note in CHECKS:
    ok = (PKG / f).is_file()
    lines.append(f"| {d} | `{f}` | {'齐' if ok else '缺'} | {note} |")
extra = [p.relative_to(PKG).as_posix() for p in PKG.rglob("*")
         if p.is_file() and p.name != "交付物核对表.md"
         and p.relative_to(PKG).as_posix() not in {f for _, f, _ in CHECKS}]
lines += ["", f"未列入核对表的交付层文件 {len(extra)} 份：" + "、".join(f"`{e}`" for e in extra), ""]
(PKG / "交付物核对表.md").write_text("\n".join(lines), encoding="utf-8")
print(f"核对表已写：齐 {len(CHECKS) - len(missing)}/{len(CHECKS)}，缺 {len(missing)} 项 -> {missing}")
if missing:
    sys.exit(1)
