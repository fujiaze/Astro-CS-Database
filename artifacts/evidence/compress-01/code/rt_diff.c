/* COMPRESS-01: FITS 压缩往返差异诊断 (NaN 处理 / 逐位一致性) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "fitsio.h"
#define NX 512
#define NY 512
#define NPIX (NX*NY)
static const char* ferr(int st){static char b[80];ffgerr(st,b);return b;}
int main(int argc,char**argv){
  const char* bin=argv[1]; const char* cn=argv[2];
  int ctype=-1, q0=0; char base[32]; snprintf(base,sizeof base,"%s",cn);
  { char*p=strstr(base,"_q0"); if(p){*p=0;q0=1;} }
  if(!strcmp(base,"RICE_1"))ctype=RICE_1; else if(!strcmp(base,"GZIP_1"))ctype=GZIP_1;
  else if(!strcmp(base,"GZIP_2"))ctype=GZIP_2; else if(!strcmp(base,"HCOMPRESS_1"))ctype=HCOMPRESS_1;
  else {printf("unknown codec\n");return 1;}
  FILE* f=fopen(bin,"rb"); unsigned char* raw=malloc(NPIX*4);
  if(fread(raw,1,NPIX*4,f)!=NPIX*4){printf("bad size\n");return 1;} fclose(f);
  uint32_t* orig=malloc(NPIX*4);
  for(int i=0;i<NPIX;i++) orig[i]=((uint32_t)raw[i*4]<<24)|((uint32_t)raw[i*4+1]<<16)|((uint32_t)raw[i*4+2]<<8)|raw[i*4+3];
  int st=0; fitsfile *in=NULL,*out=NULL;
  remove("/tmp/rt_raw.fits"); remove("/tmp/rt_cmp.fits");
  fits_create_file(&in,"/tmp/rt_raw.fits",&st);
  long naxes[2]={NX,NY}; fits_create_img(in,FLOAT_IMG,2,naxes,&st);
  fits_write_img(in,TFLOAT,1,NPIX,raw,&st);  /* raw 是 BE, cfitsio 会自动按机器序处理? 不: 用 TFLOAT 需本机序 */
  fits_close_file(in,&st);
  /* 重新按本机序写, 保证正确 */
  remove("/tmp/rt_raw.fits"); st=0;
  float* nat=malloc(NPIX*4); for(int i=0;i<NPIX;i++) memcpy(&nat[i],&orig[i],4);
  fits_create_file(&in,"/tmp/rt_raw.fits",&st);
  fits_create_img(in,FLOAT_IMG,2,naxes,&st);
  fits_write_img(in,TFLOAT,1,NPIX,nat,&st);
  fits_close_file(in,&st);
  st=0; fits_open_file(&in,"/tmp/rt_raw.fits",READONLY,&st);
  fits_create_file(&out,"/tmp/rt_cmp.fits",&st);
  fits_set_compression_type(out,ctype,&st);
  if(q0) fits_set_quantize_level(out,0.0f,&st);
  long td[2]={NX,NY}; fits_set_tile_dim(out,2,td,&st);
  if(fits_img_compress(in,out,&st)){printf("compress fail: %s\n",ferr(st));return 1;}
  fits_close_file(out,&st); fits_close_file(in,&st);
  st=0; fitsfile* cf=NULL; float* back=malloc(NPIX*4);
  fits_open_file(&cf,"/tmp/rt_cmp.fits",READONLY,&st);
  fits_movabs_hdu(cf,2,NULL,&st);
  /* 用显式 nulval 控制读回 NaN 的位模式 (nulval=NULL 时 cfitsio 用自己的规范 NaN) */
  uint32_t nanbits = 0x7FC00000u; float nanval; memcpy(&nanval,&nanbits,4);
  fits_read_img(cf,TFLOAT,1,NPIX,&nanval,back,NULL,&st);
  fits_close_file(cf,&st);
  uint32_t* b32=(uint32_t*)back;
  int diff=0, nan_in=0, nan_out=0, nan_lost=0, nan_gained=0;
  for(int i=0;i<NPIX;i++){
    uint32_t o=orig[i], b=b32[i];
    int on=((o&0x7F800000u)==0x7F800000u)&&((o&0x007FFFFFu)!=0);
    int bn=((b&0x7F800000u)==0x7F800000u)&&((b&0x007FFFFFu)!=0);
    if(on)nan_in++; if(bn)nan_out++;
    if(on&&!bn)nan_lost++; if(!on&&bn)nan_gained++;
    if(o!=b)diff++;
  }
  printf("%-16s %-14s diff=%d/%d nan_in=%d nan_out=%d nan_lost=%d nan_gained=%d\n",bin,cn,diff,NPIX,nan_in,nan_out,nan_lost,nan_gained);
  int shown=0;
  for(int i=0;i<NPIX&&shown<4;i++) if(orig[i]!=b32[i]){ printf("    i=%d orig=0x%08X back=0x%08X\n",i,orig[i],b32[i]); shown++; }
  return 0;
}