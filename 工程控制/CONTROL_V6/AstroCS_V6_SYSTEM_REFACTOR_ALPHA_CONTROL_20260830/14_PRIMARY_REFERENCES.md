# 一手科学与工程参考

本表不是“引用越多越正确”。SCI/ALG 只能引用实际采用的定义，并必须写出 AstroCS 的具体推导、单位和差异；禁止用论文标题替代本项目合同。

## 科学

1. A. S. Fruchter, R. N. Hook, **Drizzle: A Method for the Linear Reconstruction of Undersampled Images**, PASP 114, 144–152 (2002).  
   https://arxiv.org/abs/astro-ph/9808087  
   用于 Drizzle footprint、权重、光度与几何畸变语义。AstroCS 的球面/HEALPix实现仍须独立推导。

2. K. M. Górski et al., **HEALPix: A Framework for High-Resolution Discretization and Fast Analysis of Data Distributed on the Sphere**, ApJ 622, 759–771 (2005).  
   https://arxiv.org/abs/astro-ph/0409513  
   用于 HEALPix 等面积、层次与 ordering 语义。

3. E. W. Greisen, M. R. Calabretta, **Representations of world coordinates in FITS** (Paper I), A&A 395 (2002).  
   M. R. Calabretta, E. W. Greisen, **Representations of celestial coordinates in FITS** (Paper II).  
   NASA FITS WCS 索引：https://fits.gsfc.nasa.gov/fits_wcs.html  
   Paper II：https://arxiv.org/abs/astro-ph/0207413  
   用于 Phase3 WCS/header/projection；同时检查 NASA 页列出的 errata。

4. J. C. Jacob et al., **Montage: a grid portal and software toolkit for science-grade astronomical image mosaicking** (2010).  
   https://arxiv.org/abs/1005.4454  
   Montage algorithms：https://montage.ipac.caltech.edu/docs/algorithms.html  
   用于重投影、背景差分拟合、rectification 与模块化 mosaic 工程参考。AstroCS 冻结为加性 UPM，不能因此引入未批准的乘性/全局平坦化。

5. M. P. Maples et al., **Robust Chauvenet Outlier Rejection** (2018).  
   https://arxiv.org/abs/1807.05276  
   仅当代码完整实现 RCR 定义时引用；不得把任意 sigma clip 命名为 RCR。

## FITS 与并发

6. NASA/HEASARC CFITSIO C User’s Guide, **Using CFITSIO in Multi-threaded Environments**.  
   https://heasarc.gsfc.nasa.gov/docs/software/fitsio/c/c_user/node15.html  
   工程合同：构建 reentrant；现场 `fits_is_reentrant()`；同一只读 FITS 可由不同线程独立 open；不得共享同一 `fitsfile*`；不同线程写不同文件才可并行。

7. NASA/HEASARC CFITSIO, **Multiple Access to the Same FITS File**.  
   https://heasarc.gsfc.nasa.gov/docs/software/fitsio/c/c_user/node30.html

## 并行与 ISA

8. OpenMP 5.2 Specification, implementation-defined behavior.  
   https://www.openmp.org/spec-html/5.2/openmpap1.html  
   线程动态调整、亲和性和部分调度行为可能由实现决定，因此 AstroCS 必须现场记录实际 worker/affinity，不能只相信请求值。

9. OpenMP `omp_get_max_threads` definition.  
   https://www.openmp.org/spec-html/5.0/openmpsu112.html  
   该值只是新 team 的上界，不等于实际可用 CPU 或实际 active threads；Runtime 还需核验配额和运行指标。

10. GCC, **Function Multiversioning**.  
    https://gcc.gnu.org/onlinedocs/gcc/Function-Multiversioning.html  
    说明多版本函数/运行时解析能力。AstroCS 为保持 GCC/MSVC 和 DLL/so 一致，首选稳定 provider C ABI；不把 GCC 特有机制作为唯一产品架构。

11. Intel 64 and IA-32 Architectures Software Developer/Optimization Manuals.  
    https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html  
    https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html#optimization  
    用于 CPUID、OSXSAVE/XGETBV、SIMD 与优化分析。代码不得只凭 CPU 品牌/型号启用指令。

## 引用验收

- 每份 SCI/ALG 的 reference 段写“采用的具体章节/公式/语义”和“与 AstroCS 的差异”。
- 链接不可访问时保留 DOI/arXiv/正式题名，不以二手博客替代。
- 论文引用只能支持科学/算法定义；源码行为仍由 TEST 和现场证据验证。
