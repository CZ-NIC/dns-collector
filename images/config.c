/*
 *	Image Library -- Configuration
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 */

#undef LOCAL_DEBUG

#include "lib/lib.h"
#include "lib/conf.h"
#include "images/images.h"
#ifndef CONFIG_FREE
#include "images/signature.h"
#endif

#include <string.h>

/* ImageLib section */
uns image_trace;
uns image_max_dim = 0xffff;
uns image_max_bytes = ~0U;

#ifndef CONFIG_FREE
/* ImageSig section */
uns image_sig_min_width;
uns image_sig_min_height;
uns *image_sig_prequant_thresholds;
uns image_sig_postquant_min_steps;
uns image_sig_postquant_max_steps;
uns image_sig_postquant_threshold;
double image_sig_border_size;
int image_sig_border_bonus;
double image_sig_inertia_scale[3];
double image_sig_textured_threshold;
int image_sig_compare_method;
uns image_sig_cmp_features_weights[IMAGE_REG_F + IMAGE_REG_H];
#endif

static struct cf_section image_lib_config = {
  CF_ITEMS{
    CF_UNS("Trace", &image_trace),
    CF_UNS("ImageMaxDim", &image_max_dim),
    CF_UNS("ImageMaxBytes", &image_max_bytes),
    CF_END
  }
};

#ifndef CONFIG_FREE
static struct cf_section image_sig_config = {
  CF_ITEMS{
    CF_UNS("MinWidth", &image_sig_min_width),
    CF_UNS("MinHeight", &image_sig_min_height),
    CF_UNS_DYN("PreQuantThresholds", &image_sig_prequant_thresholds, CF_ANY_NUM),
    CF_UNS("PostQuantMinSteps", &image_sig_postquant_min_steps),
    CF_UNS("PostQuantMaxSteps", &image_sig_postquant_max_steps),
    CF_UNS("PostQuantThreshold", &image_sig_postquant_threshold),
    CF_DOUBLE("BorderSize", &image_sig_border_size),
    CF_INT("BorderBonus", &image_sig_border_bonus),
    CF_DOUBLE_ARY("InertiaScale", image_sig_inertia_scale, 3),
    CF_DOUBLE("TexturedThreshold", &image_sig_textured_threshold),
    CF_LOOKUP("CompareMethod", &image_sig_compare_method, ((byte *[]){"integrated", "fuzzy", "average", NULL})),
    CF_UNS_ARY("CompareFeaturesWeights", image_sig_cmp_features_weights, IMAGE_REG_F + IMAGE_REG_H),
    CF_END
  }
};
#endif

static void CONSTRUCTOR
images_init_config(void)
{
  cf_declare_section("ImageLib", &image_lib_config, 0);
#ifndef CONFIG_FREE
  cf_declare_section("ImageSig", &image_sig_config, 0);
#endif
}
