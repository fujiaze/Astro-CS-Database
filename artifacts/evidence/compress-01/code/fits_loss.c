/* COMPRESS-01: FITS 有损档 (fpack 默认 -q 4) 的量化误差测量 —— 判据非退化对照.
 * 真值无效应 => 若编码无损, 所有误差必须为 0 (负例红/绿).
 * 用法: fits_loss <file.bin> <codec>   (codec: RICE_1 | GZIP_1 | GZIP_2 | *_q0)
 * 输出: diff_px, nan_lost, nan_gained, max_rel_err, med_rel_err, p99_rel_err
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "fitsio.h"
#define NX 512
#define NY 512
#define NPIX (NX*NY)
static const char* ferr(int st){static char b[FLEN_ERRMSG];ffgerr(st,b);return b;}
static int cmpd(const void*a,const void*b){double x=*(double*)a,y=*(double*)b;return (x>y)-(x<y);}
int main(int argc,char**argv){
  const char* bin=argv[1]; const char* cn=argv[2];
  int ctype=-1,q0=0; char base[32]; snprintf(base,sizeof base,"%s",cn);
  { char*p=strstr(base,"_q0"); if(p){*p=0;q0=1;} }
  if(!strcmp(base,"RICE_1"))ctype=RICE_1; else if(!strcmp(base,"GZIP_1"))ctype=GZIP_1;
  else if(!strcmp(base,"GZIP_2"))ctype=GZIP_2; else {printf("unknown\n");return 1;}
  FILE* f=fopen(bin,"rb"); unsigned char* raw=malloc(NPIX*4);
  if(!f||fread(raw,1,NPIX*4,f)!=NPIX*4){printf("%s: bad size\n",bin);return 1;} fclose(f);
  uint32_t* ob=malloc(NPIX*4);
  for(int i=0;i<NPIX;i++) ob[i]=((uint32_t)raw[i*4]<<24)|((uint32_t)raw[i*4+1]<<16)|((uint32_t)raw[i*4+2]<<8)|raw[i*4+3];
  float* nat=malloc(NPIX*4); for(int i=0;i<NPIX;i++) memcpy(&nat[i],&ob[i],4);
  int st=0; fitsfile *in=NULL,*out=NULL;
  remove("/tmp/fl_raw.fits"); remove("/tmp/fl_cmp.fits");
  fits_create_file(&in,"/tmp/fl_raw.fits",&st);
  long naxes[2]={NX,NY}; fits_create_img(in,FLOAT_IMG,2,naxes,&st);
  fits_write_img(in,TFLOAT,1,NPIX,nat,&st); fits_close_file(in,&st);
  st=0; fits_open_file(&in,"/tmp/fl_raw.fits",READONLY,&st);
  fits_create_file(&out,"/tmp/fl_cmp.fits",&st);
  fits_set_compression_type(out,ctype,&st);
  if(q0) fits_set_quantize_level(out,0.0f,&st);
  long td[2]={NX,NY}; fits_set_tile_dim(out,2,td,&st);
  if(fits_img_compress(in,out,&st)){printf("%s,%s,COMPRESS_FAIL,%s\n",bin,cn,ferr(st));return 0;}
  fits_close_file(out,&st); fits_close_file(in,&st);
  st=0; fitsfile* cf=NULL; float* back=malloc(NPIX*4);
  fits_open_file(&cf,"/tmp/fl_cmp.fits",READONLY,&st);
  fits_movabs_hdu(cf,2,NULL,&st);
  fits_read_img(cf,TFLOAT,1,NPIX,NULL,back,NULL,&st);
  fits_close_file(cf,&st);
  uint32_t* b32=(uint32_t*)back;
  int diff=0,nan_lost=0,nan_gain=0,nfin=0;
  double* errs=malloc(sizeof(double)*NPIX);
  for(int i=0;i<NPIX;i++){
    uint32_t o=ob[i],b=b32[i];
    int on=((o&0x7F800000u)==0x7F800000u)&&((o&0x007FFFFFu)!=0);
    int bn=((b&0x7F800000u)==0x7F800000u)&&((b&0x007FFFFFu)!=0);
    if(on&&!bn)nan_lost++; if(!on&&bn)nan_gain++;
    if(o!=b)diff++;
    if(!on&&!bn){ float fo,fb; memcpy(&fo,&o,4); memcpy(&fb,&b,4);
      double d=fabs((double)fb-(double)fo); double r=d/(fabs((double)fo)+1e-300);
      errs[nfin++]=r; }
  }
  qsort(errs,nfin,sizeof(double),cmpd);
  printf("%s,%s,diff_px=%d,nan_lost=%d,nan_gained=%d,n_finite=%d,max_rel=%.3e,med_rel=%.3e,p99_rel=%.3e\n",
    bin,cn,diff,nan_lost,nan_gain,nfin, nfin?errs[nfin-1]:0.0, nfin?errs[nfin/2]:0.0, nfin?errs[(int)(nfin*0.99)]:0.0);
  return 0;
}
