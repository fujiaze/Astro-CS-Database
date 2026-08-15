# AstroCS 项目交接文档

**更新**: 2026-08-15（V18R2 资源驱动性能优化 + 代码收尾）
**分支**: main ｜ **HEAD**: `77fc48e`（V18R2 性能提交）

## 1. 项目总览

AstroCS 是天体图像处理内核（C++17/DLL）：将真实 FITS 经校准、plate
solve、PSF、测光、SNR、Drizzle 投影为标准化 IVOA HiPS 球面数据库，
再由 Phase2 以 UPM 全局校准 + 稳健排异 + 加权积分生成连续马赛克。

- 主仓库: https://github.com/fujiaze/Astro-CS-Database
- Wiki: https://github.com/fujiaze/Astro-CS-Database.wiki.git（独立 submodule）
- 权威规范: `工程控制/` + `docs/`；代码唯一修改入口 `lib/`
- 运行产物统一写 `run/`；原始数据只读 `testdata/`

## 2. 生产入口（唯一）

```text
Phase1  : lib/orchestrator/cpp/orchestrator.exe <stage1.json>
Phase2  : lib/phase2/tools/astrocs-stage2.exe <stage2.json>
Browser : lib/healpix_db/healpix_browser_qt/healpix_browser_qt.exe
工具链  : toolchain.ps1（check/build/run/review）
```

## 3. 当前状态（V18R2，2026-08-15）

### 性能（资源驱动，before 冻结基线 129.7/126.1/126.65s）

```text
最终完整 16 帧 wall median 67.35s（-47%），16/16 rc=0
RSS 峰值 37.5GB → 1.2GB（-97%）
进程退出延迟 40s → 0.7s

关键修复（根因）：
  - gaia 极区查询 RA 环绕 bbox 退化 → 全树遍历 16.3GB/13s/查询
    → 极投影平面剪枝：35MB/0.03s，星集 899/899 逐颗一致
  - spectrum 查询同病（PHOTOMETRIC 17.8s → <1s）
  - Drizzle：fine profiler 门控、thread-local scratch、行级 Vec3 缓存、
    安全余量 dot 预判、run constants、4 角 boundary array
  - HiPS：dtype scratch 复用、hierarchy NESTED 直通
  - SNR model RAII（消除 HiPS-only 路径 malloc 泄漏）
```

### 代码收尾（V18R2 已提交）

```text
SHA-256 归一化到 lib/common/crypto（删除 orchestrator + ACR 3 份重复实现）
lib/data_pipeline 删除（canonical = astro_image_io PipelineFrame）
drizzle omp_set_num_threads → parallel num_threads 子句（无全局副作用）
orchestrator logger 默认路径清理（run/logs 由 config 注入）
```

## 4. 模块地图

```text
lib/common            HEALPix core（唯一 NESTED 映射）、crypto（唯一 SHA-256）
lib/astro_image_io    FITS/XISF/HiPS 读写（唯一 I/O）、PipelineFrame
lib/calibration       master bias/dark/flat
lib/plate_solve/cpp/ipv  plate solve（ipv_solver.dll + star_detector）
lib/gaia_xpsd_client  Gaia DR3/DR3SP 本地 mmap 查询（XPSD）
lib/dynamic_psf       PSF 拟合
lib/photometric_calib 测光校准（GaiaDR3SP 光谱）
lib/snr_estimator     SNR catalogue
lib/healpix_db/healpix_drizzle  Drizzle（order-7 HiPS 直写）
lib/healpix_db/healpix_browser_qt  HiPS 浏览器
lib/phase2            UPM/采样/排异/积分/马赛克（astrocs-stage2）
lib/acr               异构计算基座（KernelRegistry 后端；同一科学 contract）
lib/star_detector     星点检测（SDET）
```

## 5. 数据与外部依赖

```text
testdata/     7 数据集、710 亮场、27 母版（只读）
GaiaDR3/      41.9GB 星表（本地，gitignored，禁止删除）
GaiaDR3SP/    64.7GB 光谱 XPSD（本地，gitignored，禁止删除）
BASS DR3/     BASS DR3 备用数据集索引（China-VO 镜像，直连无代理；
              downloads/ 未存在；V19 才下载实测）
siril-1.4.3/  Siril 源码（oracle 参考，GPL ORACLE ONLY）
```

## 6. 冻结与待办

```text
已冻结：Phase1 基础算法（V14 后）、Phase2 排异/积分语义（V17）、
        WBPP 2.9.1 路由政策、HiPS 序列化（V11）
V18R2 完成：性能收尾（资源驱动）+ 代码收尾（重复实现/API/docs）
V19 计划（未执行）：GC/t4/16-exposure 回归、2×2/3×3 合成、
        BASS real 2×2/3×3、最终 foundation freeze
```

## 7. 历史与归档

```text
历史控制包/审核包归档在 archive_deliverables/（ACR 包禁止删除）
审核包命名：AstroCS_Review_<主题>_<版本>.zip
```
