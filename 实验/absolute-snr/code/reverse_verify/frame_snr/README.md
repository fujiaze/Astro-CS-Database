# frame_snr —— 帧级 SNR 定案的实验与红线测试

> 工作项 **FRAME-SNR-CANON**（RELEASE-02）。**纯 Python，无外部构建依赖**，
> 因此 **未在 `reverse_verify/CMakeLists.txt` 登记**（该文件只登记 C++ Oracle 目标；
> 本目录不动它，避免与并行工作项冲突）。
> 定案文档：`实验/absolute-snr/docs/frame-snr-canon.md`；调研记录：`实验/absolute-snr/docs/surveys/frame-snr-survey.md`。

## 文件

| 文件 | 作用 |
|---|---|
| `frame_snr_canon.py` | 解析内核：离散归一化 Moffat4、`A_NEA`、逐像素方差、最优提取方差、canon SNR、三个**红例定义**、合成帧 |
| `frame_snr_physical.py` | **物理前向模型**仿真（Poisson 源/天光/暗流 + 高斯读出 + 增益量化 + 平场 + 梯度）与 ACSD 等价估计量 |
| `run_redlines.py` | 解析红线 T8–T12（不依赖外部库） |
| `run_redlines_physical.py` | 物理红线 P8–P14（**核心**） |
| `crosscheck_photutils.py` | 与 `photutils` / `sep` / SExtractor(源码级) 对拍 |
| `inventory_p1_snr.py` | 扫描 `run/**/p1_snr.json`：C1（`snr_f == flux/sigma_f`）/ C2（`F_ref` 组内公共）/ C4（`m_5` 产出）/ C5（字段自文档化） |
| `branch_discriminator.py` | **分支判别**：用逐源表判生产走 gain 还是 gain-free（同 FWHM bin 内 `sigma_F` 是否随通量变化），含红对照 |
| `cpp/p1snr_probe.cpp` | `g++` 直编生产 TU `snr_science.cpp`（**只读**）做 C ABI 对拍 |
| `run_all.sh` | 一键复跑 |

## 复跑

```bash
export TMPDIR=/dev/shm/astrocs_fsnr
bash 实验/absolute-snr/code/reverse_verify/frame_snr/run_all.sh
```

外部对拍库（可选，缺省则该两项登记为 UNAVAILABLE）：

```bash
python3 -m pip install --quiet --target /dev/shm/astrocs_fsnr/frame_snr_canon/pylibs photutils sep
```

（本机 PEP 668 禁止系统级 pip，故用 `--target` 装到 `/dev/shm`。）

## 判据先行的纪律

- 每个测试的**判据与阈值写死在源码顶部/docstring 里**，脚本只输出 PASS/FAIL，**不事后放宽**。
- 红线测试**必须能红能绿**：绿例（canon）与红例（未扣背景 / 功率比 / 未扣背景窗口）用
  **同一个** `monotone_criterion()` 判，红例必须被否决。
- 一次例外（**已显式登记，不是放宽**）：P8 的 `B→∞` 极限改由**解析式**断言，
  因为 `B>=10^6` 时单次实现的 SNR 散度恒为 1，有限次 MC 的中位数无法分辨真值（详见 canon §3.1）。

## 产物

写 `run/reverse_verify/frame_snr/`（gitignore）：

- `redlines.json`、`redlines_physical.json`、`external_crosscheck.json`、`p1_snr_inventory.json`。

## 诚实登记

- **本工作区无哈勃数据**（全仓 `find -iname '*hst*' / '*hubble*'` 命中 0）；真实数据作底改用
  `run/RELEASE-02/perf-drz/norm_t2_m1_red/cleaned_M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts`。
- **SExtractor 二进制对拍未做**（本环境无 `sex`/`extract`）——只做源码级核对。
- 本目录**不修改** `lib/` `docs/` `eng/tests/` `ci/`，**零 git 写**。
