#!/usr/bin/env python3
"""ABI-003 安全 loader 验收测试编排（Linux amd64）。

规格: 控制包 tasks/02_ABI_BUILD_CLI_TASKS.md ABI-003:
  Windows 受控绝对路径 + SetDefaultDllDirectories/AddDllDirectory/
  LoadLibraryExW 安全 flags; Linux 仅从 product manifest 绝对 canonical path
  dlopen; 加载前后校验路径/hash/module ID/ABI/build ID。
验收:
  1. 当前目录/PATH DLL 劫持拒绝（相对路径入参即拒; 仅 manifest 绝对路径可加载）;
  2. symlink escape 拒绝（canonical 不一致 + allowed_root 越界均拒）;
  3. hash mismatch 拒绝（manifest sha256 与实际不符）;
  4. 缺 symbol 拒绝（无 astrocs_module_query_v1 的 .so）;
  5. wrong arch/ABI 拒绝（非 ELF / 32 位 / 非 x86-64 / host_abi 失配）;
  6. 日志不泄凭据（loader 无文件写入; 错误消息为静态字面量不含路径/sha; 测试断言）;
  7. 正测: 加载真实 BLD-003 astrocs_noop.so, module_id/version/build/hash 校验一致;
     describe 往返 + release。

本文件职责: 编译 fixture 与探针（gcc -shared 最小 .so; gcc 编译探针）、编排
场景、断言输出。直接运行 `python3 tests/abi/test_secure_loader.py` 退出码 0=全过。
"""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
INC = os.path.join(REPO, "include")
LOADER_DIR = os.path.join(REPO, "runtime", "module_loader")
PROBE_C = os.path.join(REPO, "tests", "abi", "abi003_loader_probe.c")
TIMEOUT = 300
CC = os.environ.get("CC", "gcc")

FAILURES = []
CHECKS_TOTAL = [0]


def run(cmd, cwd=None, env=None):
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=cwd, env=env,
                       timeout=TIMEOUT)
    return r


def check(name, cond, detail=""):
    tag = "PASS" if cond else "FAIL"
    print(f"[{tag}] {name}" + (f"  {detail}" if detail else ""))
    CHECKS_TOTAL[0] += 1
    if not cond:
        FAILURES.append(name)


# ─────────── fixture 源码 ───────────

GOOD_C = r"""
/* good module fixture: 仿 BLD-003 noop 的唯一导出入口 + describe */
#include "astrocs/abi/module_api_v1.h"
#include <string.h>
static const char kMid[] = "astrocs.test.good";
static const char kVer[] = "0.0.0-test";
static const char kBid[] = "ABI-003-fixture-good";
static acs_str_v1 sv(const char* s){acs_str_v1 v;v.head.struct_size=(uint32_t)sizeof(acs_str_v1);
  v.head.abi_version=ACS_ABI_VERSION_V1;v.data=s;v.size=(uint64_t)strlen(s);return v;}
static acs_status gd_describe(const acs_module_api_v1* self, acs_str_v1 mid,
                              acs_module_descriptor_v1* out){
  (void)self; if(!out) return ACS_ERR_PARAM;
  memset(out,0,sizeof(*out));
  out->head.struct_size=(uint32_t)sizeof(acs_module_descriptor_v1);
  out->head.abi_version=ACS_ABI_VERSION_V1;
  out->module_id=sv(kMid); out->version=sv(kVer); out->build_id=sv(kBid);
  out->sci_id=sv("SCI-NONE"); out->alg_id=sv("ALG-NONE"); out->api_id=sv("API-ABI-001");
  out->phase=0; out->config_schema_ver=1; out->execution_class=2; out->parallel_ok=0; out->flags=0;
  if(mid.data!=NULL && mid.size>0){
    if(mid.size!=strlen(kMid)||memcmp(mid.data,kMid,mid.size)!=0) return ACS_ERR_PARAM;
  }
  return ACS_OK;
}
static acs_status gd_validate(const acs_module_api_v1* self, acs_str_v1 cfg,
                              acs_error_info_v1* e){(void)self;(void)cfg;if(e){e->status=ACS_ERR_UNSUPPORTED;e->domain=ACS_ERR_DOMAIN_CONFIG;}return ACS_ERR_UNSUPPORTED;}
static acs_status gd_plan(const acs_module_api_v1* self, acs_str_v1 n, acs_str_v1 cfg,
                          acs_strbuf_v1* o, acs_error_info_v1* e){(void)self;(void)n;(void)cfg;(void)o;if(e){e->status=ACS_ERR_UNSUPPORTED;e->domain=ACS_ERR_DOMAIN_CONFIG;}return ACS_ERR_UNSUPPORTED;}
static acs_status gd_create(const acs_module_api_v1* self, acs_str_v1 cfg,
                            const acs_host_api_v1* h, acs_module_instance_v1** o,
                            acs_error_info_v1* e){(void)self;(void)cfg;(void)h;if(o)*o=NULL;if(e){e->status=ACS_ERR_UNSUPPORTED;e->domain=ACS_ERR_DOMAIN_CONFIG;}return ACS_ERR_UNSUPPORTED;}
static acs_status gd_exec(acs_module_instance_v1* i, acs_str_v1 a, acs_str_v1 b,
                          acs_strbuf_v1* o, acs_error_info_v1* e){(void)i;(void)a;(void)b;(void)o;if(e){e->status=ACS_ERR_UNSUPPORTED;e->domain=ACS_ERR_DOMAIN_CONFIG;}return ACS_ERR_UNSUPPORTED;}
static acs_status gd_inspect(const acs_module_instance_v1* i, acs_strbuf_v1* o,
                             acs_error_info_v1* e){(void)i;(void)o;if(e){e->status=ACS_ERR_UNSUPPORTED;e->domain=ACS_ERR_DOMAIN_INTERNAL;}return ACS_ERR_UNSUPPORTED;}
static acs_status gd_cancel(acs_module_instance_v1* i){(void)i;return ACS_OK;}
static void gd_destroy(acs_module_instance_v1* i){(void)i;}
static const acs_module_api_v1 g_api = {
  {(uint32_t)sizeof(acs_module_api_v1), ACS_ABI_VERSION_V1},
  gd_describe, gd_validate, gd_plan, gd_create, gd_exec, gd_inspect, gd_cancel, gd_destroy};
#if defined(ASTROCS_ABI_SHARED) && !defined(ASTROCS_ABI_EXPORTS)
#define ASTROCS_ABI_EXPORTS 1
#endif
ASTROCS_EXPORT acs_status ASTROCS_CALL
astrocs_module_query_v1(uint32_t host_abi, const acs_host_api_v1* host,
                        const acs_module_api_v1** out){
  if(host_abi!=ACS_ABI_VERSION_V1) return ACS_ERR_ABI_MISMATCH;
  if(!host||!host->allocator) return ACS_ERR_ABI_MISMATCH;
  if(!out) return ACS_ERR_PARAM;
  *out=&g_api; return ACS_OK;
}
"""

MISSING_SYMBOL_C = r"""
/* 缺 astrocs_module_query_v1 导出的 .so（wrong symbol 场景） */
int acs_unrelated_export(void){ return 42; }
"""

WRONG_HOST_ABI_C = r"""
/* query 硬拒 host_abi != 99（wrong ABI 场景）: 恒返回 ABI_MISMATCH */
#include "astrocs/abi/module_api_v1.h"
static const acs_module_api_v1 g_api = {
  {(uint32_t)sizeof(acs_module_api_v1), ACS_ABI_VERSION_V1}, 0,0,0,0,0,0,0,0};
#if defined(ASTROCS_ABI_SHARED) && !defined(ASTROCS_ABI_EXPORTS)
#define ASTROCS_ABI_EXPORTS 1
#endif
ASTROCS_EXPORT acs_status ASTROCS_CALL
astrocs_module_query_v1(uint32_t host_abi, const acs_host_api_v1* host,
                        const acs_module_api_v1** out){
  (void)host; if(out) *out=NULL;
  if(host_abi != 99) return ACS_ERR_ABI_MISMATCH;
  *out=&g_api; return ACS_OK;
}
"""

WRONG_MODULE_ID_C = r"""
/* describe 返回与 manifest 不符的 module_id（三方一致拒绝） */
#include "astrocs/abi/module_api_v1.h"
#include <string.h>
static const char kMid[] = "astrocs.test.wrong";
static acs_str_v1 sv(const char* s){acs_str_v1 v;v.head.struct_size=(uint32_t)sizeof(acs_str_v1);
  v.head.abi_version=ACS_ABI_VERSION_V1;v.data=s;v.size=(uint64_t)strlen(s);return v;}
static acs_status wd_describe(const acs_module_api_v1* self, acs_str_v1 mid,
                              acs_module_descriptor_v1* out){
  (void)self;(void)mid; if(!out) return ACS_ERR_PARAM;
  memset(out,0,sizeof(*out));
  out->head.struct_size=(uint32_t)sizeof(acs_module_descriptor_v1);
  out->head.abi_version=ACS_ABI_VERSION_V1;
  out->module_id=sv(kMid); out->version=sv("0.0.0-test"); out->build_id=sv("ABI-003-fixture-wrong");
  out->api_id=sv("API-ABI-001");
  return ACS_OK;
}
static const acs_module_api_v1 g_api = {
  {(uint32_t)sizeof(acs_module_api_v1), ACS_ABI_VERSION_V1},
  wd_describe, 0,0,0,0,0,0,0};
#if defined(ASTROCS_ABI_SHARED) && !defined(ASTROCS_ABI_EXPORTS)
#define ASTROCS_ABI_EXPORTS 1
#endif
ASTROCS_EXPORT acs_status ASTROCS_CALL
astrocs_module_query_v1(uint32_t host_abi, const acs_host_api_v1* host,
                        const acs_module_api_v1** out){
  if(host_abi!=ACS_ABI_VERSION_V1) return ACS_ERR_ABI_MISMATCH;
  if(!host||!host->allocator) return ACS_ERR_ABI_MISMATCH;
  if(!out) return ACS_ERR_PARAM;
  *out=&g_api; return ACS_OK;
}
"""

WRONG_BUILD_ID_C = r"""
/* describe 返回与 manifest 期望不符的 build_id */
#include "astrocs/abi/module_api_v1.h"
#include <string.h>
static const char kMid[] = "astrocs.test.good";
static acs_str_v1 sv(const char* s){acs_str_v1 v;v.head.struct_size=(uint32_t)sizeof(acs_str_v1);
  v.head.abi_version=ACS_ABI_VERSION_V1;v.data=s;v.size=(uint64_t)strlen(s);return v;}
static acs_status bd_describe(const acs_module_api_v1* self, acs_str_v1 mid,
                              acs_module_descriptor_v1* out){
  (void)self;(void)mid; if(!out) return ACS_ERR_PARAM;
  memset(out,0,sizeof(*out));
  out->head.struct_size=(uint32_t)sizeof(acs_module_descriptor_v1);
  out->head.abi_version=ACS_ABI_VERSION_V1;
  out->module_id=sv(kMid); out->version=sv("0.0.0-test"); out->build_id=sv("WRONG-BUILD");
  out->api_id=sv("API-ABI-001");
  return ACS_OK;
}
static const acs_module_api_v1 g_api = {
  {(uint32_t)sizeof(acs_module_api_v1), ACS_ABI_VERSION_V1},
  bd_describe, 0,0,0,0,0,0,0};
#if defined(ASTROCS_ABI_SHARED) && !defined(ASTROCS_ABI_EXPORTS)
#define ASTROCS_ABI_EXPORTS 1
#endif
ASTROCS_EXPORT acs_status ASTROCS_CALL
astrocs_module_query_v1(uint32_t host_abi, const acs_host_api_v1* host,
                        const acs_module_api_v1** out){
  if(host_abi!=ACS_ABI_VERSION_V1) return ACS_ERR_ABI_MISMATCH;
  if(!host||!host->allocator) return ACS_ERR_ABI_MISMATCH;
  if(!out) return ACS_ERR_PARAM;
  *out=&g_api; return ACS_OK;
}
"""


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def compile_so(src, out, extra_defs=()):
    """编译 fixture .so（只导出入口符号; 定义 ASTROCS_ABI_SHARED 使 ASTROCS_EXPORT
    在 Linux 展开 visibility(default)）"""
    cmd = [CC, "-std=c11", "-Wall", "-fPIC", "-shared",
           f"-I{INC}", "-DASTROCS_ABI_SHARED=1", "-DASTROCS_ABI_EXPORTS=1"]
    for d in extra_defs:
        cmd.append(f"-D{d}")
    cmd += [src, "-o", out]
    return run(cmd)


def main():
    work = tempfile.mkdtemp(prefix="abi003_")
    print(f"workdir={work}")
    try:
        # ── 编译 loader 与探针 ──
        loader_o = os.path.join(work, "secure_loader.o")
        r = run([CC, "-std=c11", "-Wall", "-fno-exceptions", "-fPIC", "-c",
                 f"-I{INC}", f"-I{LOADER_DIR}",
                 os.path.join(LOADER_DIR, "secure_loader.c"), "-o", loader_o])
        check("compile loader", r.returncode == 0, r.stderr[-400:] if r.stderr else "")
        if r.returncode != 0:
            return 1
        probe = os.path.join(work, "abi003_probe")
        r = run([CC, "-std=c11", "-Wall", "-fno-exceptions",
                 f"-I{INC}", f"-I{LOADER_DIR}",
                 os.path.join(REPO, "tests", "abi", "abi003_loader_probe.c"),
                 loader_o, "-ldl", "-o", probe])
        check("compile probe", r.returncode == 0, r.stderr[-400:] if r.stderr else "")
        if r.returncode != 0:
            return 1

        # ── 0. 自检 ──
        r = run([probe, "selftest"])
        check("selftest sha256 vectors", r.returncode == 0 and "SELFTEST_OK" in r.stdout,
              r.stdout + r.stderr)

        # ── fixture 编译 ──
        good_src = os.path.join(work, "good.c")
        with open(good_src, "w") as f:
            f.write(GOOD_C)
        good_so = os.path.join(work, "good.so")
        r = compile_so(good_src, good_so)
        check("compile good.so", r.returncode == 0, r.stderr[-300:] if r.stderr else "")

        miss_src = os.path.join(work, "missing_sym.c")
        with open(miss_src, "w") as f:
            f.write(MISSING_SYMBOL_C)
        miss_so = os.path.join(work, "missing_sym.so")
        r = compile_so(miss_src, miss_so)
        check("compile missing_sym.so", r.returncode == 0)

        wabi_src = os.path.join(work, "wrong_abi.c")
        with open(wabi_src, "w") as f:
            f.write(WRONG_HOST_ABI_C)
        wabi_so = os.path.join(work, "wrong_abi.so")
        r = compile_so(wabi_src, wabi_so)
        check("compile wrong_abi.so", r.returncode == 0)

        wmid_src = os.path.join(work, "wrong_mid.c")
        with open(wmid_src, "w") as f:
            f.write(WRONG_MODULE_ID_C)
        wmid_so = os.path.join(work, "wrong_mid.so")
        r = compile_so(wmid_src, wmid_so)
        check("compile wrong_mid.so", r.returncode == 0)

        wbid_src = os.path.join(work, "wrong_bid.c")
        with open(wbid_src, "w") as f:
            f.write(WRONG_BUILD_ID_C)
        wbid_so = os.path.join(work, "wrong_bid.so")
        r = compile_so(wbid_src, wbid_so)
        check("compile wrong_bid.so", r.returncode == 0)

        if not os.path.exists(good_so):
            return 1

        good_sha = sha256_file(good_so)

        # 复制 good.so 一份到"攻击者当前目录"(劫持场景: 传相对/同名义路径)
        cwd_dir = os.path.join(work, "cwd_attack")
        os.makedirs(cwd_dir)
        shutil.copy(good_so, os.path.join(cwd_dir, "good.so"))

        # ── 正测 1: 绝对 canonical manifest 路径 + 正确 sha/module_id/build → LOAD_OK ──
        r = run([probe, "load", "module", good_so, "astrocs.test.good", good_sha,
                 "ABI-003-fixture-good", os.path.dirname(good_so), "1"])
        check("P1 load good absolute+hash+mid+build",
              r.returncode == 0 and "LOAD_OK" in r.stdout and "RELEASE_OK" in r.stdout,
              r.stdout + r.stderr)
        check("P1 loaded sha matches manifest",
              re.search(r"sha=([0-9a-f]{64})", r.stdout) and
              re.search(r"sha=([0-9a-f]{64})", r.stdout).group(1) == good_sha,
              r.stdout)
        check("P1 loaded module_id",
              "module_id=astrocs.test.good" in r.stdout, r.stdout)
        check("P1 describe roundtrip",
              "DESCRIBE_OK" in r.stdout, r.stdout)

        # ── 正测 2: 期望 sha 为空(manifest SKELETON 未登记)也允许加载已登记 ID ──
        r = run([probe, "load", "module", good_so, "astrocs.test.good", "-",
                 "ABI-003-fixture-good", "-", "1"])
        check("P2 load without sha (manifest skeleton)",
              r.returncode == 0 and "LOAD_OK" in r.stdout, r.stdout + r.stderr)

        # ── 负测 N1: 当前目录/PATH 劫持 —— 相对路径入参拒 ──
        r = run([probe, "load", "module", "good.so", "astrocs.test.good", good_sha,
                 "ABI-003-fixture-good", os.path.dirname(good_so), "1"], cwd=cwd_dir)
        check("N1 relative path (cwd hijack) rejected",
              "LOAD_FAIL" in r.stdout and "detail=2" in r.stdout,
              r.stdout + r.stderr)   # PATH_NOT_ABS

        # ── 负测 N2: hash mismatch —— 改一个字节后 sha 不符 ──
        tampered = os.path.join(work, "tampered.so")
        data = bytearray(open(good_so, "rb").read())
        # 翻转 .so 中部一个字节(避开 ELF 头)
        data[len(data) // 2] ^= 0x01
        open(tampered, "wb").write(bytes(data))
        r = run([probe, "load", "module", tampered, "astrocs.test.good", good_sha,
                 "ABI-003-fixture-good", os.path.dirname(tampered), "1"])
        check("N2 tampered file hash mismatch rejected",
              "LOAD_FAIL" in r.stdout and "detail=5" in r.stdout,
              r.stdout + r.stderr)

        # ── 负测 N3: 缺必需 symbol 拒 ──
        r = run([probe, "load", "module", miss_so, "astrocs.test.good", "-",
                 "-", os.path.dirname(miss_so), "1"])
        check("N3 missing entry symbol rejected",
              "LOAD_FAIL" in r.stdout and "detail=11" in r.stdout,
              r.stdout + r.stderr)

        # ── 负测 N4: wrong host ABI(握手拒; query 恒 ABI_MISMATCH) ──
        r = run([probe, "load", "module", wabi_so, "astrocs.test.good", "-",
                 "-", os.path.dirname(wabi_so), "1"])
        check("N4 host ABI handshake rejected",
              "LOAD_FAIL" in r.stdout and "detail=12" in r.stdout,
              r.stdout + r.stderr)

        # ── 负测 N5: manifest module_id 与 describe 不符拒 ──
        r = run([probe, "load", "module", wmid_so, "astrocs.test.expected", "-",
                 "-", os.path.dirname(wmid_so), "1"])
        check("N5 module_id mismatch rejected",
              "LOAD_FAIL" in r.stdout and "detail=15" in r.stdout,
              r.stdout + r.stderr)

        # ── 负测 N6: manifest build_id 与 describe 不符拒 ──
        r = run([probe, "load", "module", wbid_so, "astrocs.test.good", "-",
                 "ABI-003-fixture-good", os.path.dirname(wbid_so), "1"])
        check("N6 build_id mismatch rejected",
              "LOAD_FAIL" in r.stdout and "detail=16" in r.stdout,
              r.stdout + r.stderr)

        # ── 负测 N7: symlink escape ──
        #  7a: 路径本身是 symlink → canonical != 入参 → PATH_NOT_CANONICAL
        linkdir = os.path.join(work, "linkdir")
        os.makedirs(linkdir)
        linkpath = os.path.join(linkdir, "good_link.so")
        os.symlink(good_so, linkpath)
        r = run([probe, "load", "module", linkpath, "astrocs.test.good", good_sha,
                 "ABI-003-fixture-good", os.path.dirname(good_so), "1"])
        check("N7a symlink path rejected (not canonical)",
              "LOAD_FAIL" in r.stdout and "detail=3" in r.stdout,
              r.stdout + r.stderr)
        #  7b: 真实文件在 allowed_root 外 → PATH_ESCAPE
        outside = os.path.join(work, "outside")
        os.makedirs(outside)
        outside_so = os.path.join(outside, "good.so")
        shutil.copy(good_so, outside_so)
        # allowed_root 为 work 下另一授权目录(不含 outside/) → 文件越界
        allowed_root = os.path.join(work, "allowed_root")
        os.makedirs(allowed_root)
        r = run([probe, "load", "module", outside_so, "astrocs.test.good", good_sha,
                 "ABI-003-fixture-good", allowed_root, "1"])
        check("N7b file outside allowed_root rejected (escape)",
              "LOAD_FAIL" in r.stdout and "detail=4" in r.stdout,
              r.stdout + r.stderr)

        # ── 负测 N8: wrong arch —— 非 ELF / 32 位 / 非 x86-64 ──
        notelf = os.path.join(work, "notelf.so")
        with open(notelf, "wb") as f:
            f.write(b"MZ\x90\x00" + b"\x00" * 60 + b"not a real module")
        r = run([probe, "load", "module", notelf, "-", "-", "-",
                 os.path.dirname(notelf), "1"])
        check("N8a non-ELF rejected",
              "LOAD_FAIL" in r.stdout and "detail=8" in r.stdout,
              r.stdout + r.stderr)
        elf32 = os.path.join(work, "elf32.so")
        # 伪造 ELF32 头: magic+class=1(32 位)+data=1+machine x86
        hdr = bytearray(20)
        hdr[0:4] = b"\x7fELF"
        hdr[4] = 1   # ELFCLASS32
        hdr[5] = 1   # LSB
        hdr[18:20] = (3).to_bytes(2, "little")  # EM_386
        with open(elf32, "wb") as f:
            f.write(bytes(hdr) + b"\x00" * 64)
        r = run([probe, "load", "module", elf32, "-", "-", "-",
                 os.path.dirname(elf32), "1"])
        check("N8b ELF32 rejected",
              "LOAD_FAIL" in r.stdout and "detail=9" in r.stdout,
              r.stdout + r.stderr)
        elfarm = os.path.join(work, "elfarm.so")
        hdr2 = bytearray(20)
        hdr2[0:4] = b"\x7fELF"
        hdr2[4] = 2   # ELFCLASS64
        hdr2[5] = 1
        hdr2[18:20] = (183).to_bytes(2, "little")  # EM_AARCH64
        with open(elfarm, "wb") as f:
            f.write(bytes(hdr2) + b"\x00" * 64)
        r = run([probe, "load", "module", elfarm, "-", "-", "-",
                 os.path.dirname(elfarm), "1"])
        check("N8c non-x86-64 ELF rejected",
              "LOAD_FAIL" in r.stdout and "detail=10" in r.stdout,
              r.stdout + r.stderr)

        # ── 负测 N9: wrong abi_version 期望(manifest 期望 ABI=99) ──
        r = run([probe, "load", "module", good_so, "astrocs.test.good", good_sha,
                 "ABI-003-fixture-good", os.path.dirname(good_so), "99"])
        check("N9 manifest abi_version 99 rejected",
              "LOAD_FAIL" in r.stdout and "detail=14" in r.stdout,
              r.stdout + r.stderr)

        # ── 负测 N10: 文件不存在 ──
        r = run([probe, "load", "module",
                 os.path.join(work, "no_such_file.so"), "-", "-", "-", "-", "1"])
        check("N10 missing file rejected",
              "LOAD_FAIL" in r.stdout and "detail=6" in r.stdout,
              r.stdout + r.stderr)

        # ── 负测 N11: kind 不支持(恶意 unit.kind) ──
        r = run([probe, "load", "kernel", good_so, "-", "-", "-", "-", "1"])
        check("N11 unsupported kind rejected",
              "LOAD_FAIL" in r.stdout and "detail=17" in r.stdout,
              r.stdout + r.stderr)

        # ── 日志不泄凭据: 错误消息字面量不含路径与 sha ──
        loader_c = open(os.path.join(LOADER_DIR, "secure_loader.c")).read()
        detail_msgs = re.findall(r'return "loader:[^"]*"', loader_c)
        # 静态断言消息为固定字面量: 无格式化参数(无 %s 插值 → 无路径/sha 注入面)
        fmt_leaks = [m for m in detail_msgs if "%" in m]
        check("L1 error messages are static literals (no path/sha interpolation)",
              not fmt_leaks, str(fmt_leaks))
        # loader 不写任何文件/日志: fopen 只允许 "rb" 只读; 独立 open( 只允许 dlopen(
        write_fopen = re.findall(r'fopen\s*\(\s*[^,]*,\s*"(?:w|a)[^"]*"', loader_c)
        bad_open = re.findall(r'(?<![a-z])open\(', loader_c)  # 排除 fopen/dlopen
        check("L2 loader has no file writer (no log/write fopen)",
              not write_fopen and not bad_open,
              f"write_fopen={write_fopen} bad_open={bad_open}")

        # ── 回归: 无凭证/路径泄漏在 stdout/stderr(探针打印仅测试侧) ──
        r = run([probe, "load", "module", tampered, "astrocs.test.good", good_sha,
                 "-", os.path.dirname(tampered), "1"])
        check("L3 reject output free of path+sha",
              "LOAD_FAIL" in r.stdout and
              good_sha[:16] not in r.stdout and tampered not in r.stdout,
              r.stdout)

        # ── 正测 P3: 加载 BLD-003 真实 noop 模块(独立 gcc 编译, 不依赖仓库先 build) ──
        noop_src = os.path.join(REPO, "modules", "conformance", "noop", "src",
                                "noop_module.c")
        noop_so = os.path.join(work, "astrocs_noop.so")
        r = compile_so(noop_src, noop_so)
        check("P3 compile noop real module", r.returncode == 0,
              r.stderr[-300:] if r.stderr else "")
        if r.returncode == 0:
            noop_sha = sha256_file(noop_so)
            # module.yaml 权威: module_id=astrocs.conformance.noop / version=0.11.0-alpha.1
            # noop_module.c describe: module_id=astrocs.conformance.noop build_id=BLD-003-skeleton
            r = run([probe, "load", "module", noop_so,
                     "astrocs.conformance.noop", noop_sha,
                     "BLD-003-skeleton", os.path.dirname(noop_so), "1"])
            check("P3 loader loads noop.so with manifest hash+mid+build",
                  r.returncode == 0 and "LOAD_OK" in r.stdout and
                  "RELEASE_OK" in r.stdout and "DESCRIBE_OK" in r.stdout,
                  r.stdout + r.stderr)
            check("P3 noop module_id matches module.yaml",
                  "module_id=astrocs.conformance.noop" in r.stdout, r.stdout)
            check("P3 noop build matches BLD-003-skeleton",
                  "build=BLD-003-skeleton" in r.stdout, r.stdout)
            check("P3 noop loaded sha matches computed",
                  re.search(r"sha=([0-9a-f]{64})", r.stdout) and
                  re.search(r"sha=([0-9a-f]{64})", r.stdout).group(1) == noop_sha,
                  r.stdout)
            # noop 真实模块 + manifest 期望 sha 错误 → 拒
            r = run([probe, "load", "module", noop_so,
                     "astrocs.conformance.noop", "0" * 64,
                     "BLD-003-skeleton", os.path.dirname(noop_so), "1"])
            check("P3b noop wrong manifest hash rejected",
                  "LOAD_FAIL" in r.stdout and "detail=5" in r.stdout,
                  r.stdout + r.stderr)

        print(f"\n{'='*60}\nresults: {len(FAILURES)} FAIL / "
              f"{CHECKS_TOTAL[0] - len(FAILURES)} PASS "
              f"(total {CHECKS_TOTAL[0]} checks)\n{'='*60}")
        if FAILURES:
            print("FAILED:", *FAILURES, sep="\n  ")
            return 1
        return 0
    finally:
        if os.environ.get("KEEP_ABI003_WORK"):
            print(f"KEEP workdir: {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
