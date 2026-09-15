# W1 片4 · P2｜F_TEST_GAP 与 H_NUMERIC

### W1-N-10（P2）新增只写不读面：`p1drz_thread_probe.cpp` 写 `<prefix>.order`，但**两把新锁都不消费 `.order`** ⇒ P15a 的输出序规范化无锁守护
- probe 自述 `.order` 是「tiles 向量返回顺序＝tile directory 写盘顺序，用于逐位比对」；而 `p1drz_merge_pipeline_lock.sh` 只比 `.canon` 与 `.norm.hiss`、`p1drz_taskset_invariance.sh` 只比 `.norm.hiss` ⇒ **能守 `.order` 的采集面为零**。
- **后果**：P15a 新加的 `std::sort(tiles, parent_ipix)`（输出序规范化）**没有守护**——若被回退或改成不稳定排序，两把锁仍绿。related 机制⑪、簇 6、`V9-N-17`（taskset 锁本身另有 exit-0 问题）

### W1-N-12（P2）`HEALPIX_SCALE_PER_NSIDE_ARCSEC` 双实现，无单一承载层
- `drizzle_engine.cpp:684`（注释自称权威「禁魔数」计算）与 `module_adapters.cpp:3146`（P17 块内同式重推导）⇒ **同值双实现，后续单边改式即漂**。related `V12-N-01`/`V12-N-16`、`S-1`（单一权威点）
