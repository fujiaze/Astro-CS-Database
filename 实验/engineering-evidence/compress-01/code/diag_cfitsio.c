/* 诊断: cfitsio float 图像"无损"压缩路径的可用 API 组合 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "fitsio.h"

#define NX 512
#define NY 512
#define NPIX (NX*NY)

int main(int argc, char** argv) {
  const char* bin = argv[1];
  FILE* f = fopen(bin, "rb");
  unsigned char* raw = malloc(NPIX*4);
  if (fread(raw,1,NPIX*4,f) != NPIX*4) { printf("bad size\n"); return 1; }
  fclose(f);
  float* nat = malloc(NPIX*4);
  for (int i=0;i<NPIX;i++){uint32_t v=((uint32_t)raw[i*4]<<24)|((uint32_t)raw[i*4+1]<<16)|((uint32_t)raw[i*4+2]<<8)|raw[i*4+3];memcpy(&nat[i],&v,4);}
  int nn=0, nnan=0; for(int i=0;i<NPIX;i++){ if(nat[i]!=0) nn++; if(nat[i]!=nat[i]) nnan++; }
  printf("== %s: nonzero=%d nan=%d\n", bin, nn, nnan);

  const char* modes[] = {"noisebits0","quantlevel0","noisebits16"};
  int ctypes[] = {RICE_1, GZIP_1, GZIP_2};
  const char* cnames[] = {"RICE_1","GZIP_1","GZIP_2"};
  for (int m=0;m<3;m++) for (int c=0;c<3;c++) {
    int st=0; fitsfile *in=NULL,*out=NULL;
    remove("/tmp/diag_raw.fits"); remove("/tmp/diag_cmp.fits");
    fits_create_file(&in,"/tmp/diag_raw.fits",&st);
    long naxes[2]={NX,NY};
    fits_create_img(in,FLOAT_IMG,2,naxes,&st);
    fits_write_img(in,TFLOAT,1,NPIX,nat,&st);
    fits_close_file(in,&st); st=0;
    fits_open_file(&in,"/tmp/diag_raw.fits",READONLY,&st);
    fits_create_file(&out,"/tmp/diag_cmp.fits",&st);
    fits_set_compression_type(out,ctypes[c],&st);
    if (m==0) fits_set_noise_bits(out,0,&st);
    if (m==1) fits_set_quantize_level(out,0.0f,&st);
    if (m==2) fits_set_noise_bits(out,16,&st);
    long td[2]={NX,NY}; fits_set_tile_dim(out,2,td,&st);
    int rc = fits_img_compress(in,out,&st);
    long csz=-1;
    if (!rc) { FILE* g=fopen("/tmp/diag_cmp.fits","rb"); fseek(g,0,SEEK_END); csz=ftell(g); fclose(g); }
    { char eb[FLEN_ERRMSG]; eb[0]=0; if (st) ffgerr(st, eb);
      printf("  mode=%-12s codec=%-7s rc=%d status=%d comp=%ld  err=%s\n", modes[m], cnames[c], rc, st, csz, eb); }
    if (out) fits_close_file(out,&st);
    if (in) fits_close_file(in,&st);
  }
  return 0;
}