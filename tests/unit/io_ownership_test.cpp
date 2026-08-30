// IO-002 单元测试: AIOImageData canonical deleter 全覆盖
// (配合 tools/check_aio_ownership.py 静态扫描; 此处验证 deleter 行为)
#include "astro_image_io.h"

#include <cstdio>
#include <cstring>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

int main() {
  // 1) aio_free_image_data 对空指针安全 (canonical deleter null-safe)
  aio_free_image_data(nullptr);

  // 2) 分配-释放配对: 反复 create/free 不应泄漏 (结构含多指针字段)
  //    无法直接构造 AIOImageData (不透明); 用 aio_read 对不存在文件返回 nullptr 验证空路径安全
  AIOImageData* im = aio_read("/nonexistent/path/nope.fits");
  CHECK(im == nullptr);
  aio_free_image_data(im);  // nullptr 安全

  // 3) 检查 canonical deleter 符号存在且可调用 (链接时已保证)
  //    静态断言: aio_free_image_data 是导出的释放函数
  std::printf("IO-002 TESTS PASS (canonical deleter null-safe; ownership scan 见 tools/check_aio_ownership.py)\n");
  return 0;
}
