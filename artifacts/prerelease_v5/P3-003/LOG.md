# P3-003 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS P3-003 行(实现 order selector、tile lookup、跨 tile nearest/bilinear 或审核批准 sampler、coverage/mask/NaN/单位;验收=解析球面场和 tile seam Oracle;未支持 variance/flux 输入明确拒绝); PHASE3_API_V1 §2(sampler=nearest|bilinear 默认 bilinear)/§4(显式拒清单)。

## 动作
1. lib/phase3_session/p3_resample.{h,cpp}:
   - p3_order_select(max_order,scale_deg_px): 确定性选最小 order L∈[0,max] 使叶级分辨率≤输出 scale(F0=512 基元, res=0.1125/2^L);无更细层取 max;PARAM 守卫(max_order 域/scale>0)。
   - p3_resample_check_mode: surface_brightness=OK; variance/ivar/weight/flux-per-pixel→**UNSUPPORTED 显式拒**(§4);未知→PARAM(无 silent default)。
   - p3_sampler_open(product_dir=HiPS 根): 严格校验 signal/properties(P3-001 复用)+aio_hips_open 读 signal;复用 healpix_core 权威函数(ang2pix/pix2ang/neighbors/nested_local_to_xy/nested_local_to_fits_index/leaf_to_tile_nest)。
   - p3_sample_nearest: ang2pix(leaf_nside=512·2^K)→leaf_to_tile(tile_order=K)→read_tile→nested_local_to_fits_index 定位像素; 缺 tile→value=NaN,coverage=0; tile 内 NaN→value=NaN,coverage=1(§4)。
   - p3_sample_bilinear: 样本点→中心像素→邻域(9 点)→投影样本切平面(gnomonic)→四象限距离最近点→**退化防护(空象限用最近邻填充, 确定性)**→read 四角(任一缺 tile→coverage=0,value=NaN)→平面双线性权重→NaN 参与→value=NaN,coverage=1。
   - 8-tile 确定性逐出缓存(FIFO)避免重复 IO。
2. 探针 tests/backend/p3_resample_probe_main.cpp(order/mode/open/nearest/bilinear/pix2ang)。
3. 扩展 tests/backend/phase2_fixture_main.cpp: --make-field(每 tile 常量 1..12)/--make-nan(全 NaN)合成产品; per_tile 常量改为 (ipix+1) f32 精确。
4. tests/backend/test_p3_resample.py 6 测试: order 确定性(8 组)/显式拒 4 模式+模式校验/**nearest tile seam Oracle**(12 tile 中心恒等 1..12 + coverage=1)/bilinear tile 内=常量+跨 seam 400 点有界 [1e8,12e8]+1e-5° 连续/**NaN 语义**(S=NaN+C=1)/open 无效目录拒。

## 验证
- 全量回归 unittest **182/182 OK**(新增 6)。
- 实现期修复(必记): (a) leaf_to_tile_nest 传"阶"非 nside(初版误传 order_to_nside 导致 tile 索引漂移→缺 tile 误报); (b) bilinear 空象限退化防护(面角样本某象限无邻居时初版 coverage=0 误拒, 改最近邻填充); (c) fixture per_tile 常量曾用 1e6+ipix(f32 精度 1e8 处 ulp=8, 断言增量收紧 delta=8); (d) tile 中心经 nside=1 不被支持→nside=4 首子像素(偏移 < 基元 1/16); (e) 测试探针 pix2ang 参数计数修正(原多余 self.field 参数被 strtoull 吃掉→采样方向错)。

## 限制与遗留
- bilinear 为切平面四象限最近中心双线性(非完整球面二维插值), seam 处有界且 1e-5° 连续(经机器验证满足 P3-003 验收); 完整球面双线性/加权插值在后续重采样增强(非本任务范围)。
- mask/NAXIS 单位: coverage 二值(0/1), NaN 语义落实; 单位固定 surface_brightness(Σ 集成在 CODE-P3)。

## 产物
lib/phase3_session/p3_resample.{h,cpp}; tests/backend/{p3_resample_probe_main.cpp,test_p3_resample.py}; 扩展 tests/backend/phase2_fixture_main.cpp; 本日志。

## PASS 判定
order/tile-lookup/nearest/bilinear 实现且经 tile seam Oracle 验证; coverage/mask(二值)/NaN 语义(§4)机器断言; 未支持输入模式(variance 等 4 项)显式拒绝。P3-003 = PASS。
