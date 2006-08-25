/*
 *	Image Library -- Configuration
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/conf.h"
#include "images/images.h"
#include "images/signature.h"

#include <string.h>

uns image_sig_min_width;
uns image_sig_min_height;
uns *image_sig_prequant_thresholds;
uns image_sig_postquant_min_steps;
uns image_sig_postquant_max_steps;
uns image_sig_postquant_threshold;

static struct cf_section sig_config = {
  CF_ITEMS{
    CF_UNS("MinWidth", &image_sig_min_width),
    CF_UNS("MinHeight", &image_sig_min_height),
    CF_UNS_DYN("PreQuantThresholds", &image_sig_prequant_thresholds, CF_ANY_NUM),
    CF_UNS("PostQuantMinSteps", &image_sig_postquant_min_steps),
    CF_UNS("PostQuantMaxSteps", &image_sig_postquant_max_steps),
    CF_UNS("PostQuantThreshold", &image_sig_postquant_threshold),
    CF_END
  }
};

static void CONSTRUCTOR
images_init_config(void)
{
  cf_declare_section("ImageSig", &sig_config, 0);
}
