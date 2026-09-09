/* adapter_entry_impl.cpp - hips_writer_adapter_test 的 direct adapter 通道
 *
 * 对齐先例: tests/unit/drizzle_adapter_test.cpp → #include drizzle_adapter_impl.cpp
 *           tests/unit/gaia_adapter_test.c  → #include gaia_client.c
 * 本 TU 把 lib/hips/src/module_entry.cpp (astrocs_module_query_v1 唯一定义,
 * 与 DLL astrocs_p1_hips_writer 同源同文件) 编入测试可执行, 使 adapter 通道
 * 的 vtable 用例可在 direct 通道直接调用; legacy aio_hips_* 九符号由
 * astrocs_hips STATIC 提供 (与 p1_hips_writer_test 同口径)。真实 DLL 的
 * 导出面净化探针 (dlsym legacy 九符号全 NULL) 走 ASTROCS_HIPS_DLL_PATH,
 * 见 adapter_test.c case 8。
 */
#include "../../../lib/hips/src/module_entry.cpp"
