# W1 收口 · **正向控制登记**（非缺陷，勿进账本）

- 本区间 `ci/checks.json` **新增 12 门**（CTEST-IPV-ABI-LAYOUT-LOCK/-SELFCHECK/-PARAMS-ABI-FAILCLOSED/-MAG-ITER-DELIVERY、CTEST-GAIA-MAGNITUDE-RANGE-BOUNDS、CTEST-P1SNR-FRAME-PARITY/-SCIENCE-ALL/-LINUX-ALL、CTEST-P1DRZ-TASKSET-INVARIANCE/-MERGE-PIPELINE-LOCK、CTEST-P1STAR-ANGLE-GUARD、CTEST-IPV-TRIANGLE-BUDGET），W1 逐名与 `add_test` 实比对账：**除两条 glob 门外全部命中**（`p1snr_linux_*`/`p1snr_science_.*` 的目标集在已入库 p1noise 子图内 CI 可见）。
- **且新增 `ipv_abi_layout_lock_selfcheck`（反向破坏镜像证明锁非恒真）⇒ 这是我前轮 `C-21`/`E11`/`E14` 建议的自发落实，记为正向**。它与 `V11` 时点我核到的 ipv 修复同源：隔壁已从「补字段」进到「给锁配自检」。
- **注释抽检 12 处（超 10 配额）**：属实 5（ipv_api.h 5/5 入口校验、ipv_select.h 四条生产路径、module_entry 三先例文件存在、P14-N-08 precise 直调传 0、gaia_client.h 非导出面）；**不实 4**（虚构钩子 `N-02`、snr CMake 称根已注册 `N-03`、负责人裁定零登记 `N-06`、包络 16.59 口径错 `N-08`）；存疑 3（run/ 内证据锚不可仓内复核、probe 头注释输出清单缺 `.norm.hiss`、P18 注释 log_dir 96→168 两时点面均 256 不可复算）。⇒ **不实率 4/12**，与第一轮 `A-44`/`E10`（裁决与注释不进登记面）同族。
## 两条须转给隔壁（W1 终报）
- **提交原子性（真风险）**：`ipv_dead_params` 锁三件（lock 脚本 / manifest / selfcheck）**+ 两个 gate（`tests/unit/CMakeLists.txt` 与 `ci/checks.json` 均为工作区 M）** 是**在途未跟踪态** ⇒ 若跟踪面先行入库而脚本未收编，**干净树上这两道门的 command 指空**（新机制：门注册与门脚本分裂在不同跟踪态）。**前台/隔壁须在同一个 commit 内并提**；`E13`（清单项须存在且实扫数大于 0）应把「command 指向的文件在 HEAD 树中存在」纳入判据。
- **W1 的一次自纠值得记**：其初判「`CTEST-P1SNR-SCIENCE-ALL` 空转」经三级复核**主动撤回不立条**（glob 门目标集在已入库 p1noise 子图内、CI 可见）⇒ 与 `V18`/`V11` 同档的正样本。
- 其余正向闭环：ipv P18 四件套（布局锁 + selfcheck 反向锁 + 5 入口全覆盖 fail-closed + 全部调用方收口）为本区间最高质量；P22 归约流水线静态无竞态且确定性论证成立（in-process 1..16 逐位锁是真采集）；gaia 有界解析 + 414 行锁设计良好；p1snr parity 含非恒真负例。
- 需运行期未判：P22 宣称 93.5%→95.1% 并行效率；新触顶判据在真实 Gaia 密场下的触发频率；taskset 锁在未入库产物上的逐位结果；dead-params 锁为邻站在途未跟踪件（扫描中 10:10 出现，不作判）。
