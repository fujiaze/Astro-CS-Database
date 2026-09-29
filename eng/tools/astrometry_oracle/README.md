# astrometry_oracle

外部求解 WCS 的交叉验证工具族（仅非生产工具）：比对 Astrometry.net 求解结果与参考 WCS / ACSD 像素到天球链。

## 职责边界

- 放：外部求解 WCS 与参考 WCS 的指标比对、参考 WCS 参数推导、合成星场生成。
- 不放：生产 WCS 求解链、闭环指标门（在 `eng/tools/astrometry/`）、检查项注册表（在 `eng/ci/checks.json`）。

## 内容

- `compare_astrometry.py` —— 比对求解 WCS 与参考 WCS：中心、尺度、旋转与像素到天球链指标，输出报告 JSON 并给 PASS 判定。
- `make_astrocs_ref.py` —— 从 drizzle lineage 的像素四角 RA/Dec 推导参考 WCS 参数（center/scale/rotation/pixfrac）。
- `gen_synthetic_from_axy.py` —— 把检测源经注入 WCS 投影渲染为合成星场，头部不写 WCS 供盲解恢复。
- `__pycache__/` —— Python 字节码缓存，不入库。

## 上游

- 本目录工具未注册于 `eng/ci/checks.json`（交叉验证按需执行）。
- 本目录在 CI 门禁规范（工程正本内的检查项清单与门禁规范篇）无逐项对应篇；独立 Oracle 原则见 `docs/engineering/01_CHECKS.md` §1 注册表原则。
