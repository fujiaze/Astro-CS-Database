/* ============================================================================
 * fits_cross_reader.c - 独立 FITS 交叉读取器（判据 CHK-SPARSE-PUNCH-PROBE 的
 * 「第二路读器」）。**链接系统 cfitsio，不链接 AstroCS 任何库**，因此它读到的
 * 一致性与生产实现无关。
 *
 * 依据: ACCEPTANCE_SPEC.md §3.2「cfitsio 与 astropy 两路独立读器逐 HDU 的
 *       header 卡片与数据区原始字节全等」；docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md §7 T1。
 *
 * 输出（单行 JSON，stdout）: HDU 数 + 逐 HDU 的
 *   nkeys / BITPIX / NAXIS / 像素数 / 数据区字节偏移 / DATASUM 与 CHECKSUM 校验结果
 *   / header 卡片摘要 / 像素摘要（FNV-1a 64）。
 * 摘要由 cfitsio 解析路径产出（fits_read_record 逐卡、fits_read_img 逐像素），
 * 因此「打洞前后摘要相同」= cfitsio 这条路读到的内容逐字节不变。
 *
 * 本程序**不做任何文件系统原语**（无 fopen/fread/fseek/fwrite）：只经 cfitsio API
 * 读、只向 stdout 写。原始数据区字节的逐字节比对由判据侧（Python）在文件层面完成
 * —— 这是刻意的分工：本仓「aio 是文件级唯一 I/O 边界」，判据工具不得引入第二处
 * 文件 I/O 实现。
 * 退出码: 0 = 读通；3 = cfitsio 报错；2 = 参数错误。
 * ==========================================================================*/
#include <fitsio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* FNV-1a 64：确定性、无外部依赖的内容摘要（只用于「前后是否相同」的等价判据）。 */
static unsigned long long fnv1a(unsigned long long h, const void* data, size_t n) {
    const unsigned char* p = (const unsigned char*)data;
    for (size_t i = 0; i < n; ++i) {
        h ^= (unsigned long long)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: fits_cross_reader <fits>\n");
        return 2;
    }
    const char* path = argv[1];
    int status = 0;
    fitsfile* f = NULL;
    if (fits_open_file(&f, path, READONLY, &status)) {
        fits_report_error(stderr, status);
        return 3;
    }
    int nhdu = 0;
    if (fits_get_num_hdus(f, &nhdu, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(f, &status);
        return 3;
    }
    printf("{\"hdus\":%d,\"per_hdu\":[", nhdu);
    for (int h = 1; h <= nhdu; ++h) {
        if (fits_movabs_hdu(f, h, NULL, &status)) {
            fits_report_error(stderr, status);
            fits_close_file(f, &status);
            return 3;
        }
        int nkeys = 0, morekeys = 0;
        if (fits_get_hdrspace(f, &nkeys, &morekeys, &status)) {
            fits_report_error(stderr, status);
            fits_close_file(f, &status);
            return 3;
        }
        unsigned long long hdr_hash = 1469598103934665603ULL;   /* FNV offset basis */
        for (int k = 1; k <= nkeys; ++k) {
            char card[FLEN_CARD];
            card[0] = '\0';
            if (fits_read_record(f, k, card, &status)) {
                fits_report_error(stderr, status);
                fits_close_file(f, &status);
                return 3;
            }
            size_t len = strlen(card);
            if (len < 80) { memset(card + len, ' ', 80 - len); card[80] = '\0'; }
            hdr_hash = fnv1a(hdr_hash, card, 80);
        }

        int bitpix = 0, naxis = 0;
        long naxes[10];
        memset(naxes, 0, sizeof(naxes));
        if (fits_get_img_param(f, 10, &bitpix, &naxis, naxes, &status)) {
            fits_report_error(stderr, status);
            fits_close_file(f, &status);
            return 3;
        }
        long long npix = (naxis > 0) ? 1 : 0;
        for (int i = 0; i < naxis; ++i) npix *= (long long)naxes[i];
        const long long data_bytes =
            npix * (bitpix < 0 ? -(long long)bitpix : (long long)bitpix) / 8;

        LONGLONG hdroff = 0, dstart = 0, dend = 0;
        if (fits_get_hduaddrll(f, &hdroff, &dstart, &dend, &status)) {
            fits_report_error(stderr, status);
            fits_close_file(f, &status);
            return 3;
        }
        const long long hdr_blocks = ((long long)nkeys * 80 + 2879) / 2880;
        const long long dataoff = (long long)hdroff + hdr_blocks * 2880;
        if ((long long)dstart != dataoff) {
            fprintf(stderr, "data offset mismatch: cfitsio=%lld computed=%lld\n",
                    (long long)dstart, dataoff);
            fits_close_file(f, &status);
            return 3;
        }

        /* 像素经 cfitsio 读出（转换为 double 后摘要）；读失败 ⇒ 判红。 */
        unsigned long long pix_hash = 1469598103934665603ULL;
        long long read_pix = 0;
        if (npix > 0) {
            double* buf = (double*)malloc((size_t)npix * sizeof(double));
            if (!buf) {
                fprintf(stderr, "malloc failed for %lld pixels\n", npix);
                fits_close_file(f, &status);
                return 3;
            }
            int anynul = 0;
            if (fits_read_img(f, TDOUBLE, 1, npix, NULL, buf, &anynul, &status)) {
                fits_report_error(stderr, status);
                free(buf);
                fits_close_file(f, &status);
                return 3;
            }
            pix_hash = fnv1a(pix_hash, buf, (size_t)npix * sizeof(double));
            read_pix = npix;
            free(buf);
        }

        /* DATASUM / CHECKSUM（cfitsio 自带校验；无 CHECKSUM 键时返回 -1） */
        int dataok = -1, hduok = -1;
        int kstatus = 0;
        char csum[FLEN_VALUE];
        csum[0] = '\0';
        int has_csum = 0;
        if (fits_read_key(f, TSTRING, "CHECKSUM", csum, NULL, &kstatus) == 0) has_csum = 1;
        status = 0;
        if (has_csum) {
            if (fits_verify_chksum(f, &dataok, &hduok, &status)) {
                fits_report_error(stderr, status);
                fits_close_file(f, &status);
                return 3;
            }
        }
        status = 0;

        printf("%s{\"hdu\":%d,\"nkeys\":%d,\"bitpix\":%d,\"naxis\":%d,\"npix\":%lld,"
               "\"data_bytes\":%lld,\"data_offset\":%lld,\"has_checksum\":%d,"
               "\"dataok\":%d,\"hduok\":%d,\"header_fnv1a64\":\"%016llx\","
               "\"pixels_fnv1a64\":\"%016llx\",\"pixels_read\":%lld}",
               (h == 1 ? "" : ","), h, nkeys, bitpix, naxis, npix, data_bytes,
               dataoff, has_csum, dataok, hduok, hdr_hash, pix_hash, read_pix);
    }
    printf("]}\n");
    fits_close_file(f, &status);
    return 0;
}
