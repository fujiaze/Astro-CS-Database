============================================================
  AstroCS CLI Core v1.1.0 - 快速开始
============================================================

【系统要求】
  - Windows 10/11 (64-bit)
  - 无需安装 Python / PowerShell / .NET
  - 无需安装 Visual C++ Runtime
  - 最低 4 GB RAM (Stage1 单帧), 推荐 64 GB RAM (南天天区)

【目录结构】
  AstroCS-CLI-v1/
    lib/orchestrator/cpp/orchestrator.exe  -- CLI 主程序
    lib/astro_image_io/                    -- I/O 模块 DLL
    lib/calibration/                       -- 校准模块 DLL
    lib/plate_solve/cpp/ipv/               -- PlateSolve 模块 DLL
    lib/dynamic_psf/                       -- PSF 模块 DLL
    lib/photometric_calib/cpp/             -- 测光校准 DLL + Gaia 客户端
    lib/snr_estimator/cpp/                 -- SNR 估算 DLL
    lib/healpix_db/healpix_drizzle/        -- Drizzle DLL
    lib/healpix_db/healpix_stack/          -- 堆栈 DLL
    bin/                                   -- MinGW 运行时 DLL
    config/                                -- 默认配置文件
      default_stage1.json                  -- Stage1 默认配置
      default_stage2.json                  -- Stage2 默认配置
    VERSION.txt                            -- 版本信息
    SHA256SUMS.txt                         -- SHA-256 清单
    verify.bat                             -- 验证脚本

【快速验证】
  1. 打开命令提示符 (cmd.exe)
  2. 切换到发布包目录:
     cd <path>\AstroCS-CLI-v1
  3. 运行验证脚本:
     verify.bat

  验证脚本会:
    - 检查文件完整性 (SHA-256)
    - 运行 capabilities 命令
    - 运行 inspect --help

【CLI 命令】

  1. 查询能力:
     set PATH=%cd%\bin;%PATH%
     lib\orchestrator\cpp\orchestrator.exe capabilities

  2. 查看帮助:
     lib\orchestrator\cpp\orchestrator.exe --help

  3. Stage1 (单帧预处理 FITS -> .hiss):
     lib\orchestrator\cpp\orchestrator.exe stage1 --frame <fits> --output <hiss> --config config\default_stage1.json

  4. Stage2 (多帧合并 .hiss -> .hcsd):
     lib\orchestrator\cpp\orchestrator.exe stage2 --frames <dir> --output <hcsd> --config config\default_stage2.json

  5. 检查文件:
     lib\orchestrator\cpp\orchestrator.exe inspect --hiss <file>
     lib\orchestrator\cpp\orchestrator.exe inspect --hcsd <file>
     lib\orchestrator\cpp\orchestrator.exe inspect --frame <file>

  6. 运行状态:
     lib\orchestrator\cpp\orchestrator.exe status

【输出格式】
  - stdout: JSONL 事件流 (engineering/contracts/jsonl_event_schema.json)
  - stderr: 人类可读日志 (带时间戳)

【退出码】
  0   = SUCCESS
  1   = GENERIC_ERROR
  2   = DLL_LOAD_FAILED
  3   = BLOCK_MISSING
  4   = CALIBRATE_FAILED
  5   = PLATESOLVE_FAILED
  6   = DRIZZLE_FAILED
  7   = CONFIG_ERROR
  8   = FILE_IO_ERROR
  9   = TIMEOUT
  10  = CANCELLED
  20-28 = 模块特定错误
  100 = MODULE_SPECIFIC_BASE

【配置参数】
  共 49 个参数 (stage1: 34, stage2: 15), 详见:
  engineering/contracts/config_parameter_registry.csv

  关键参数:
  - gaia_data_dir: Gaia DR3SP 数据库路径 (默认 GaiaDR3SP)
  - calibration_dir: 校准帧目录 (默认 testdata/T4 calibration files)
  - threads: 线程数 (0=自动, 默认 16)
  - drizzle.pixfrac: Drizzle 像素分数 (默认 1.0)
  - stack.sigma_clip_sigma: sigma-clip 阈值 (默认 3.0)

【注意事项】
  1. GaiaDR3SP 数据库 (~50GB) 不包含在发布包中, 需单独获取
  2. 测试数据 (testdata/, ~73GB) 不包含在发布包中
  3. 运行 Stage1 前需准备校准帧 (master_bias/dark/flat)
  4. 南天天区 (如 C003) 内存需求 32-35 GB, 需 64 GB RAM
  5. HISS 文件非字节级可重现 (zstd 压缩含时间戳), 但数据一致

【技术支持】
  - 项目仓库: https://github.com/fujiaze/Astro-CS-Database
  - 架构文档: docs/ARCHITECTURE.md
  - 契约文档: engineering/contracts/

============================================================
