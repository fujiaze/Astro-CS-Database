# W6 片2 · P1 两条（复验类：C-14 扩散与导出面消费者）

### W6-N-04（P1）`C-14` 扩散复验：**struct_size 三家未做、V11 四站点全未修、镜像面仍零采集**
- 普查（受跟踪 .h/.hpp 203 个、匿名 typedef struct 计法）⇒ 124 处：**带 struct_size 仅 20**、仅带 acs_head 23、**两者皆无 81（其中 70 处在公共 include 面）**。
- 逐点核：`aio_hips.h:69 AstroSphereTileView`(8 字段) 与 `:84 AioHipsSnrPoint`(6 字段) 无；`gaia_client.h:57 GaiaSpectrumStar` 无且**该 ABI 头住在 src/ 而非 include/ ⇒ 天然逃过任何 include 普查**；`dynamic_ps​f.h:38/:17`、`star_detector.h:17 SDetParams` 无。唯一修成的是 ipv（`ipv_api.h:78 IpvParams` 有 struct_size+abi_version，靠 `fbfcfac0` + 布局锁），**但同头 `:39 IpvWcsResult` 仍无 ⇒ 同族半修**。
- `V11-N-01..04` 收口时点复测**全部未修**（11 份镜像 0 份带 struct_size）；根因未变：**这五份镜像脚本在 135 道门里引用数 = 0** ⇒ 机制⑪「零采集 ⇒ 必漂」。**正例恰是反证：补了 struct_size 又建了采集面的那一家（ipv）就不再漂。** related `C-14`、`E6`、`V11-N-01/02/03/04/10`、`V2-N-01`（VERIFIED 正对照）、`W1-N-05`

### W6-N-05（P1）导出面合同**无机器消费者**：ARC-001 只有 schema 无实例、Windows 符号门判据「非空即过」、Linux 侧零采集
- `contracts/config/module_dll_contract.schema.json`（含 `dll_units`/`dependency_matrix`/`forbidden_pairs`/`entrypoint_abi` const/`target_version` const）的**全仓消费者只有 8 份文档，`.py`/CMakeLists/ci 引用 0，也没有任何实例 JSON**；而 `cmake/install_layout.cmake` 头部写「install 树与 ARC-001 dll_units 一一对应」——**该对应不可机检**。
- 唯一触及导出符号的 `WIN-PACKAGE-CANDIDATE` 走 `ci_windows_driver.py:606+` 的 `dumpbin /EXPORTS`，**ok 条件 = exit_code==0 and bool(exports)**（只判有无导出，不比对白名单），且只覆盖 4 个 DLL；同时 `CMakeLists.txt:131/:137/:151/:228` 给 runtime/io/cpu_baseline(+noop) 设 **`WINDOWS_EXPORT_ALL_SYMBOLS ON`** ⇒ MSVC 下全顶层符号导出，**与合同「单入口」方向相反却恒绿**。
- Linux 侧：**0 道门**的命令里含「对交付 .so 跑 `nm -D` 比对导出白名单」（`nm -D` 命中只有 3 个测试源）；4 家 module 的 `.map`+`.def` 各 4 份逐字自洽（global 仅 `astrocs_module_query_v1`，正对照），`astrocs_catalog_gaia` 靠 visibility hidden，三个平台 target 无 version-script。实际导出面是否越界需真机，**未判**。related `M5b-G-14`（本条增量在 Windows 判据细节与 ARC-001 无实例两处）、`V11-N-09`、`V19-N-05`、§12.3-6、簇 1 机制③
