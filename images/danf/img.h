#ifndef __IMG_H__
#  define __IMG_H__

#  include <stdio.h>
#  include <magick/api.h>
#  include "lib/config.h"

struct ClassInfo{
  uns l, u, v;                /* average Luv coefficients */
  uns lh, hl, hh;             /* energies in Daubechies wavelet bands */
  uns count;                  /*number of blocks in this class*/
};

struct
BlockInfo{
  uns l, u, v;                /* average Luv coefficients */
  uns lh, hl, hh;             /* energies in Daubechies wavelet bands */
  u8 cls_num;                 /* number of class for this block*/
};

struct
PerturbHisInfo{
  unsigned block_num : 24;    /*24 bits for number of picture's block should be enough*/
  unsigned cls_num : 8;
};

struct
DecomposeImageInfo{
  uns max_cls_num;              /*self explaining*/
  uns threshold;                /*stopping condition*/
  uns diff_threshold;           /*stopping condition*/
  uns max_cycles;               /*max number of loops of k_means clustering algorithm*/
  uns init_decomp_num;          /*number of init decompositios */
  uns bi_len;                   /*number of image blocks*/
};

struct BlockInfo*
computeBlockInfo(PixelPacket*, uns, uns,  uns*);
uns
decomposeImage(struct DecomposeImageInfo* dii, struct BlockInfo *bi);
#endif



