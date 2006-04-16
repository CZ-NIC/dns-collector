#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <alloca.h>
#include "lib/config.h"
#include "img.h"

#define stack_alloc alloca
#define xdiff(x, y) ( ((x)>(y)) ? (x)-(y) : (y)-(x) )

//void *memset(void *s, int c, size_t n);

uns
bi_dist(struct BlockInfo *bi, struct ClassInfo *ci){
  return xdiff(bi->l, ci->l) +
         xdiff(bi->u, ci->u) +
         xdiff(bi->v, ci->v) +
         xdiff(bi->lh, ci->lh) +
         xdiff(bi->hl, ci->hl) +
         xdiff(bi->hh, ci->hh);
}


/*"simulated annealing norm" - hodnocenní úspì¹nosti simulovaného ¾íhání*/
uns
sa_norm(uns bi_len, struct BlockInfo *bi, u8 cls_num, struct ClassInfo *ci){
  uns ret=0;
  struct BlockInfo *pbi;
  
  for(pbi=bi; pbi < bi+bi_len; pbi++)
    ret+=bi_dist(pbi, &ci[pbi->cls_num]);
  return ret;
}

void
showBlockInfoAsImage(struct BlockInfo *bi, uns bi_len, uns width, uns height, Image **__image,
                     ExceptionInfo *exception){
/*  ImageInfo *image_info=CloneImageInfo((ImageInfo *) NULL);*/
/*  Image *image=AllocateImage(image_info);*/
  unsigned char *p, *q;
  uns x, y;
  
  width=(width>>2);
  height=(height>>2);
  assert(bi_len==width*height);
  p=(unsigned char*) malloc(3*(width<<2)*(height<<2)*sizeof(unsigned char));
  for (q=p, y=0; y < (height<<2); y++){
    for (x=0; x < (width<<2); x++){
      uns index=(y>>2)*width + (x>>2);
      assert(index<bi_len);
      *q++=(255/3) * (bi[index].cls_num>>2);
      *q++=(255) * ((bi[index].cls_num>>1)&0x1);
      *q++=(255) * ((bi[index].cls_num>>0)&0x1);
    }
  }
  *__image=ConstituteImage(width<<2, height<<2, "RGB", CharPixel, p, exception);
}

void
init_cls(uns cls_num, uns bi_len, u8 *cls){
  u8 *p;
  for(p=cls; p<cls+bi_len; p++)
    *p = (u8) (((double)cls_num)*rand()/(RAND_MAX+1.0));
}

void
eval_centers(uns bi_len, struct BlockInfo *bi, u8 cls_num, struct ClassInfo *ci){
  struct BlockInfo *pbi;
  struct ClassInfo *pci;

  memset((void*) ci, 0, cls_num*sizeof(struct ClassInfo));
  for(pbi=bi; pbi<bi+bi_len; pbi++){
    struct ClassInfo *pcls=&ci[pbi->cls_num];
    pcls->count++;
    pcls->l+=pbi->l;
    pcls->u+=pbi->u;
    pcls->v+=pbi->v;
    pcls->lh+=pbi->lh;
    pcls->hl+=pbi->hl;
    pcls->hh+=pbi->hh;
  }

  for(pci=ci; pci<ci+cls_num; pci++){
    uns count=pci->count;
    if(! count) continue;
    pci->l  /=count;
    pci->u  /=count;
    pci->v  /=count;
    pci->lh /=count;
    pci->hl /=count;
    pci->hh /=count;
  }
}

#ifdef DEBUG_KMEANS
void
write_BlockInfo(uns len, struct BlockInfo *bi){
  struct BlockInfo *pbi;
  for(pbi=bi; pbi<bi+len; pbi++){
    fprintf(stderr, "(%u, %u, %u, %u, %u, %u; '%u')\n", pbi->l, pbi->u, pbi->v, pbi->lh, pbi->hl, pbi->hh, pbi->cls_num);
  }
}

void
write_ClassInfo(uns len, struct ClassInfo *ci){
  struct ClassInfo *pci;
  for(pci=ci; pci<ci+len; pci++){
    fprintf(stderr, "(%u, %u, %u, %u, %u, %u; %u)\n", pci->l, pci->u, pci->v, pci->lh, pci->hl, pci->hh, pci->count);
  }
}
#endif

/*bi does not mean an array, but pointer to block we search the nearest Class for*/
u8
nearestClass(uns cls_num, struct ClassInfo *ci, struct BlockInfo *bi){
  u8 ret;
  struct ClassInfo *pci;
  uns min_dist=bi_dist(bi, ci);
  ret=0;

  for(pci=ci+1; pci<ci+cls_num; pci++){
    uns dist=bi_dist(bi, pci);
    if(dist<min_dist){
      min_dist=dist;
      ret = (u8) (pci-ci);
    }
  }
  return ret;
}

uns
__k_means(uns cls_num, struct DecomposeImageInfo* dii, struct BlockInfo* bi){
  struct ClassInfo *ci=(struct ClassInfo*) stack_alloc(cls_num*sizeof(struct ClassInfo));
  uns ret=~0U;
  uns cycle=0;
  eval_centers(dii->bi_len, bi, cls_num, ci);
  for(cycle=0; cycle<dii->max_cycles; cycle++){
    struct BlockInfo* pbi;
    for(pbi=bi; pbi<bi+dii->bi_len; pbi++){
      pbi->cls_num=nearestClass(cls_num, ci, pbi);
    }
    eval_centers(dii->bi_len, bi, cls_num, ci);
    if((ret=sa_norm(dii->bi_len, bi, cls_num, ci))<dii->threshold)
      break;
  }
  return ret;
}

void
BlockInfoTou8(struct BlockInfo *bi, u8 *cls, uns len){
  struct BlockInfo* pbi;
  u8* pcls;

  for(pbi=bi, pcls=cls; pbi<bi+len; pbi++)
    *pcls++ = pbi->cls_num;
}

void
u8ToBlockInfo(u8 *cls, struct BlockInfo *bi, uns len){
  struct BlockInfo* pbi;
  u8* pcls;

  for(pbi=bi, pcls=cls; pbi<bi+len; pbi++)
    pbi->cls_num = *pcls++;
}

uns
k_means(uns cls_num, struct BlockInfo *bi, struct DecomposeImageInfo* dii){
  u8 *act_cls=(u8*) stack_alloc(dii->bi_len*sizeof(u8));
  u8 *best_cls=(u8*) stack_alloc(dii->bi_len*sizeof(u8));
  uns best_diff=~0U, act_diff;
  uns i;

  for(i=0; i<dii->init_decomp_num; i++){
    if(i)
      init_cls(cls_num, dii->bi_len, act_cls);
    else{
    /*usually, the decompozition, that imply from here is not the best, but ensures, that the
      return values from this fuction forms nonincreasing sequence for increasing cls_num*/
      BlockInfoTou8(bi, act_cls, dii->bi_len);
      bi[(uns) (dii->bi_len*rand()/(RAND_MAX+1.0))].cls_num=cls_num-1;
    }
      
    
    u8ToBlockInfo(act_cls, bi, dii->bi_len);
    act_diff=__k_means(cls_num, dii, bi);
    if(act_diff<best_diff){
      best_diff=act_diff;
      BlockInfoTou8(bi, best_cls, dii->bi_len);
    }
    /*fprintf(stderr, "...'%u'\n", act_diff);*/
  }
  u8ToBlockInfo(best_cls, bi, dii->bi_len);
  return best_diff;
}

/*
  return: final number of classes in decomposition
*/
uns
decomposeImage(struct DecomposeImageInfo* dii, struct BlockInfo *bi){
  uns cls_num=1;
  uns err=0, old_err=0;

  {
    struct ClassInfo ci;
    struct BlockInfo *pbi;
    for(pbi=bi; pbi<bi+dii->bi_len; pbi++)
      pbi->cls_num=0;
    eval_centers(dii->bi_len, bi, 1, &ci);
    old_err=sa_norm(dii->bi_len, bi, 1, &ci);
  }
  dii->threshold=old_err>>6;
  dii->diff_threshold=old_err>>8;  
  fprintf(stderr, "\n%u; --\n", old_err);
  if(old_err<dii->threshold)
    return old_err;
  cls_num=1;
  while(cls_num<dii->max_cls_num){
    cls_num++;
    err=k_means(cls_num, bi, dii);
    fprintf(stderr, "\n(%u); %d\n", err, err-old_err);
    if(err<dii->threshold) break;
    if(err<old_err && err+dii->diff_threshold>old_err) break;
    old_err=err;
  }
  return err;
}




