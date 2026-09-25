#!/usr/bin/env python3
# CR-63 复算：把 lib/algorithms/shared/crypto/sha256.cpp 逐行照抄成 Python，
# 与 (a) 已发表的 FIPS 180-4 期望值、(b) Python 标准库 hashlib（完全独立实现）对拍。
# 不 import 仓库内任何 Python；只建模本批清单文件的算法逻辑。
import sys
import hashlib

sys.stdout.reconfigure(encoding="utf-8")

MASK = 0xFFFFFFFF

# 照抄 sha256.cpp:10-23（不改一个字符的值）
kK = [
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
    0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
    0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7,
    0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
    0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
    0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
    0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
]

# 与 FIPS 180-4 4.2.2 / 5.3.3 公布表逐字核对（独立于 hashlib 的第二道）
FIPS_K_FIRST8 = [0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
                 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5]
FIPS_K_LAST4 = [0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2]
FIPS_H0 = [0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
           0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19]


def rotr(x, n):
    # 照抄 sha256.cpp:25-27（n==0 时 C++ 是 UB：x << 32；此处如实建模为掩码后 0）
    return ((x >> n) | (x << (32 - n))) & MASK


class Sha256:
    """sha256.cpp 的逐行转写：h_/total_bits_/block_/block_len_/finalized_ 同名同语义。"""

    def __init__(self):
        self.h = list(FIPS_H0)                      # :32-34
        self.total_bits = 0
        self.block = bytearray(64)                  # :30
        self.block_len = 0
        self.finalized = False                      # :32

    def process_block(self, p):                     # :38-66
        w = [0] * 64
        for i in range(16):
            w[i] = ((p[i * 4] << 24) | (p[i * 4 + 1] << 16) |
                    (p[i * 4 + 2] << 8) | (p[i * 4 + 3]))
        for i in range(16, 64):
            s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3)
            s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10)
            w[i] = (w[i - 16] + s0 + w[i - 7] + s1) & MASK
        a, b, c, d = self.h[0], self.h[1], self.h[2], self.h[3]
        e, f, g, hh = self.h[4], self.h[5], self.h[6], self.h[7]
        for i in range(64):
            S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)
            ch = (e & f) ^ (~e & g & MASK)
            t1 = (hh + S1 + ch + kK[i] + w[i]) & MASK
            S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)
            maj = (a & b) ^ (a & c) ^ (b & c)
            t2 = (S0 + maj) & MASK
            hh = g
            g = f
            f = e
            e = (d + t1) & MASK
            d = c
            c = b
            b = a
            a = (t1 + t2) & MASK
        self.h = [(self.h[j] + v) & MASK for j, v in enumerate([a, b, c, d, e, f, g, hh])]

    def update(self, data):                         # :68-83
        if self.finalized:
            return                                  # :69 静默丢弃
        self.total_bits = (self.total_bits + len(data) * 8) & 0xFFFFFFFFFFFFFFFF
        pos = 0
        n = len(data)
        while n > 0:
            take = min(n, 64 - self.block_len)      # :73
            self.block[self.block_len:self.block_len + take] = data[pos:pos + take]
            self.block_len += take
            pos += take
            n -= take
            if self.block_len == 64:
                self.process_block(self.block)
                self.block_len = 0

    def final_hex(self):                            # :85-107
        if self.finalized:
            return "0" * 64                         # :86 二次调用返回 64 个 '0'
        self.finalized = True
        self.block[self.block_len] = 0x80           # :88
        if self.block_len + 1 > 56:                 # :89
            for i in range(self.block_len + 1, 64):
                self.block[i] = 0
            self.process_block(self.block)
            for i in range(56):
                self.block[i] = 0
        else:
            for i in range(self.block_len + 1, 56):
                self.block[i] = 0
        for i in range(8):                          # :96-97 长度大端写入 56..63
            self.block[63 - i] = (self.total_bits >> (8 * i)) & 0xFF
        self.process_block(self.block)
        out = []
        for i in range(8):                          # :102-105 每字 MSB 先出
            for b in range(28, -1, -4):
                out.append("0123456789abcdef"[(self.h[i] >> b) & 0xF])
        return "".join(out)


def theirs(data, chunks=None):
    s = Sha256()
    if chunks:
        for c in chunks:
            s.update(c)
    else:
        s.update(data)
    return s.final_hex()


PUBLISHED = {
    b"": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    b"abc": "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
    (b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"):
        "248d6a61d20638b8e5c026930c3e6039a33ce4596b8b57c1c10d24aa6cc0b55b",
    b"a" * 56: "428007d8e97af2b7391529b47c025b5154dee35e8f7f46242c256d65683ab499",
    b"a" * 64: "ffe054fe7ae0cb6dcdf33760e7adc8ab614d125f170fa3bf54fcd10e522acc62",
    b"a" * 1000000: "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
}

print("== 常量表核对（FIPS 180-4 4.2.2 / 5.3.3）==")
print("kK 项数 =", len(kK), "（应为 64）")
print("kK[:8] 与公布一致:", kK[:8] == FIPS_K_FIRST8)
print("kK[-4:] 与公布一致:", kK[-4:] == FIPS_K_LAST4)
print("H0 与公布一致:", FIPS_H0 == [0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19])

print("\n== 逐行转写 vs 已发表期望值 vs hashlib ==")
cases = [b"", b"abc", b"a", b"ab", b"\x00",
         b"a" * 55, b"a" * 56, b"a" * 57, b"a" * 63, b"a" * 64, b"a" * 65,
         b"a" * 111, b"a" * 112, b"a" * 119, b"a" * 120, b"a" * 127, b"a" * 128,
         b"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
         bytes(range(256)), b"a" * 1000000]
bad = 0
for msg in cases:
    t = theirs(msg)
    h = hashlib.sha256(msg).hexdigest()
    exp = PUBLISHED.get(msg)
    ok_h = (t == h)
    ok_p = (exp is None) or (t == exp)
    if not (ok_h and ok_p):
        bad += 1
    label = f"{len(msg)}B" if len(msg) < 200 else f"{len(msg)}B"
    print(f"{label:>10} | 照抄==hashlib: {'OK ' if ok_h else 'FAIL'}"
          f" | 照抄==发表值: {'OK' if ok_p else 'FAIL'}{'' if exp else ' (无发表值,仅hashlib)'}"
          f" | 发表值==hashlib: {'OK' if exp in (None, h) else 'FAIL'}")

print("\n== 分块 update（流式路径 :68-83）==")
stream_bad = 0
for total in (1, 63, 64, 65, 130, 1000, 4097):
    data = bytes((i * 7) & 0xFF for i in range(total))
    for cut in (1, 2, 3, 7, 63, 64, 65):
        if cut > total + 1:
            continue
        chunks = [data[i:i + cut] for i in range(0, total, cut)]
        t = theirs(data, chunks=chunks)
        if t != hashlib.sha256(data).hexdigest():
            stream_bad += 1
            print(f"FAIL total={total} cut={cut}")
print("分块不一致条数 =", stream_bad)

print("\n== 二次 final / final 后 update 的实际返回（:69,:86 建模）==")
s = Sha256()
s.update(b"abc")
first = s.final_hex()
second = s.final_hex()
s.update(b"abc")
third = s.final_hex()
print("第一次 final_hex =", first)
print("第二次 final_hex =", second, "== 64 个 '0':", second == "0" * 64)
print("final 后 update 再 final  =", third, "== 64 个 '0':", third == "0" * 64)
print("真实摘要（hashlib）      =", hashlib.sha256(b"abc").hexdigest())
print("=> 64 个 '0' 是否像一个合法摘要（长度/字符集）:",
      len(second) == 64 and all(c in "0123456789abcdef" for c in second))

print("\n总结：算法主体不一致条数 =", bad, " 流式不一致条数 =", stream_bad)
