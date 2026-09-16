#include "aio_pipeline.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

static int pass=0, failc=0;
#define CHECK(c,m) do{ if(c){std::printf("[PASS] %s\n",m);pass++;}else{std::printf("[FAIL] %s\n",m);failc++;}}while(0)

template<class T> static void put(std::ofstream& f, T v){f.write(reinterpret_cast<const char*>(&v),sizeof(v));}
static void str(std::ofstream& f,const char*s){int32_t n=(int32_t)std::strlen(s);put(f,n);f.write(s,n);} 

static void make_bad_negative_blocks(const char* p){
 std::ofstream f(p,std::ios::binary); f.write("AIO1",4); put<int32_t>(f,1); put<int32_t>(f,-1); put<int32_t>(f,0);
}
static void make_bad_five_dims(const char* p){
 std::ofstream f(p,std::ios::binary); f.write("AIO1",4); put<int32_t>(f,1); put<int32_t>(f,1); put<int32_t>(f,0);
 str(f,"data"); put<int32_t>(f,AIO_BLOCK_FLOAT32); put<int32_t>(f,5);
 for(int i=0;i<5;i++) put<int32_t>(f,1);
 put<int64_t>(f,1); put<float>(f,1.0f); str(f,"x");
}
static void make_truncated_after_header(const char* p){
 std::ofstream f(p,std::ios::binary); f.write("AIO1",4); put<int32_t>(f,1); put<int32_t>(f,2); put<int32_t>(f,7); str(f,"data");
}

int main(){
 std::setvbuf(stdout,nullptr,_IONBF,0);
 std::printf("sizeof(AioKVEntry)=%zu sizeof(AioBlock)=%zu sizeof(PipelineFrame)=%zu align(AioBlock)=%zu\n",sizeof(AioKVEntry),sizeof(AioBlock),sizeof(PipelineFrame),alignof(AioBlock));
 CHECK(sizeof(AioKVEntry)==320,"AioKVEntry layout 64+256 bytes");
 CHECK(offsetof(AioBlock,data)>offsetof(AioBlock,type),"AioBlock field order stable");
 CHECK(AIO_BLOCK_FLOAT32==0 && AIO_BLOCK_FLOAT64==1 && AIO_BLOCK_RAW==6,"AioBlockType enum values stable");
 PipelineFrame* f=aio_pipeline_frame_create(); CHECK(f!=nullptr,"frame create");
 int dims[2]={2,3}; float a[6]={1,2,3,4,5,6};
 CHECK(aio_frame_add_block(f,"data",AIO_BLOCK_FLOAT32,a,6,dims,2,"f32")==0,"add f32 data");
 a[0]=99; auto*b=aio_frame_get_block(f,"data");
 CHECK(b && b->type==AIO_BLOCK_FLOAT32 && b->count==6 && ((float*)b->data)[0]==1,"add_block deep copy and dtype");
 double d[6]={-1,0,1,2,3,4};
 CHECK(aio_frame_add_block(f,"data",AIO_BLOCK_FLOAT64,d,6,dims,2,"f64 replace")==0,"replace block f32->f64");
 b=aio_frame_get_block(f,"data"); CHECK(b&&b->type==AIO_BLOCK_FLOAT64&&((double*)b->data)[0]==-1,"replacement dtype/data correct");
 CHECK(aio_frame_kv_set(f,"header","PRECISION","fp64")==0,"KV set string");
 CHECK(aio_frame_kv_set_double(f,"header","CRVAL1",123.456)==0,"KV set double");
 CHECK(std::strcmp(aio_frame_kv_get(f,"header","PRECISION"),"fp64")==0,"KV get string");
 CHECK(std::abs(aio_frame_kv_get_double(f,"header","CRVAL1",0)-123.456)<1e-12,"KV get double");
 int32_t* moved=(int32_t*)std::malloc(4*sizeof(int32_t)); for(int i=0;i<4;i++)moved[i]=i+10; int md[1]={4};
 CHECK(aio_frame_add_block_move(f,"ids",AIO_BLOCK_INT32,moved,4,md,1,"moved")==0,"add_block_move");
 CHECK(aio_frame_get_block_data(f,"ids")==moved,"move preserves pointer ownership");
 f->stages_completed=0x35;
 const char* cache="/tmp/astrocs_pipeline_contract.aio";
 CHECK(aio_frame_save_cache(f,cache)==0,"cache save");
 PipelineFrame* g=aio_pipeline_frame_create();
 CHECK(aio_frame_load_cache(g,cache)==0,"cache load");
 CHECK(g->stages_completed==0x35 && g->n_blocks==3,"cache stages/block count roundtrip");
 auto* gd=aio_frame_get_block(g,"data"); CHECK(gd&&gd->type==AIO_BLOCK_FLOAT64&&gd->dims[0]==2&&gd->dims[1]==3,"cache dtype/dims roundtrip");
 CHECK(std::memcmp(gd->data,d,sizeof(d))==0,"cache f64 payload bit exact");
 CHECK(aio_frame_remove_block(g,"ids")==0&&!aio_frame_has_block(g,"ids"),"remove block");
 CHECK(aio_frame_remove_block(g,"missing")==1,"remove missing returns 1");

 make_bad_negative_blocks("/tmp/bad_neg_blocks.aio");
 int rc=aio_frame_load_cache(g,"/tmp/bad_neg_blocks.aio");
 CHECK(rc!=0,"reject negative n_blocks");

 // restore a known block, then verify a failed load is transactional.
 aio_frame_add_block(g,"sentinel",AIO_BLOCK_INT32,md,1,md,1,"sentinel");
 make_truncated_after_header("/tmp/bad_trunc.aio");
 rc=aio_frame_load_cache(g,"/tmp/bad_trunc.aio");
 CHECK(rc!=0,"reject truncated cache");
 CHECK(aio_frame_has_block(g,"sentinel")==1,"failed cache load preserves prior frame atomically");

 make_bad_five_dims("/tmp/bad_5dims.aio");
 rc=aio_frame_load_cache(g,"/tmp/bad_5dims.aio");
 CHECK(rc!=0,"reject n_dims > 4 without stream misalignment");

 // invalid public arguments
 CHECK(aio_frame_add_block(f,"bad",AIO_BLOCK_FLOAT32,nullptr,-1,nullptr,0,"")!=0,"reject negative count");
 CHECK(aio_frame_add_block(f,"badtype",(AioBlockType)99,nullptr,1,nullptr,0,"")!=0,"reject unknown type");
 CHECK(aio_frame_add_block_move(f,"badmove",(AioBlockType)99,nullptr,1,nullptr,0,"")!=0,"move API rejects unknown type");

 aio_pipeline_frame_destroy(g); aio_pipeline_frame_destroy(f);
 std::printf("SUMMARY pass=%d fail=%d\n",pass,failc);
 return failc?1:0;
}
