#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <alloca.h>
#include "lib/config.h"
#include "img.h"

int
main(int argc, char *argv[]){
  if(argc<2)
    return 0;
  
  u8 retval=0;
  struct DecomposeImageInfo dii;
  ExceptionInfo exception;
  Image *image=NULL;
  ImageInfo *image_info=NULL;
  struct BlockInfo *bi=NULL;

  Image *out_image=NULL;

  InitializeMagick(NULL);
  GetExceptionInfo(&exception);
  
  image_info=CloneImageInfo((ImageInfo *) NULL);
  (void) strcpy(image_info->filename, argv[1]);
  image=ReadImage(image_info,&exception);
  if(exception.severity != UndefinedException)
    CatchException(&exception);

  if(!image){
    fprintf(stderr, "Invalid image format");
    goto exit;
  }
  if(image->columns < 4 || image->rows < 4){
    fprintf(stderr, "Image too small (%dx%d)", (int)image->columns, (int)image->rows);
    retval = -1;
    goto exit;
  }

  QuantizeInfo quantize_info;
  GetQuantizeInfo(&quantize_info);
  quantize_info.colorspace = RGBColorspace;
  QuantizeImage(&quantize_info, image);

  PixelPacket *pixels = (PixelPacket *) AcquireImagePixels(image, 0, 0, image->columns, image->rows, &exception);
  if (exception.severity != UndefinedException) CatchException(&exception);
  bi=computeBlockInfo(pixels, image->columns, image->rows, &dii.bi_len);
  
  dii.max_cls_num=16;
  //dii.threshold=100;
  //dii.diff_threshold=1000;
  dii.max_cycles=7;
  dii.init_decomp_num=5;
  
  decomposeImage(&dii, bi);

  showBlockInfoAsImage(bi, dii.bi_len, image->columns, image->rows, &out_image, &exception);
  if (exception.severity != UndefinedException) CatchException(&exception);
  
  image_info=CloneImageInfo((ImageInfo *) NULL);
  strcpy(out_image->filename, "/proc/self/fd/1"); /*out_image->file=stdout did'n work for me*/
  out_image->compression=JPEGCompression;
  if(WriteImage(image_info, out_image)==0)
    CatchException(&out_image->exception);
exit:
  DestroyImage(out_image);
  DestroyImage(image);
  DestroyImageInfo(image_info);
  DestroyExceptionInfo(&exception);
  DestroyMagick();
  return retval;
}
