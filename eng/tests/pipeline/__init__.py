"""阶段管线层包标记。

本层是 std/05 六层中的 pipeline 层：normalize / mosaic / export 各自阶段内
流程（调度→模块链→块生命周期→产物 manifest 落盘），不跨阶段、不跑全链。
跨阶段与全链由 synthetic / e2e 承接，写域不重叠。

骨架复用：注册/断言/证据/非阻塞裁决复用 `eng.tests.unit.harness`（只读复用，
本层不写 unit 层一字）。容差冻结表是本层自己的 `tolerances.py`，与别层不同源。
"""

from __future__ import annotations
