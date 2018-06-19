/*
 *	Image Library -- Configuration
 *
 *	(c) 2006 Pavel Charvat <pchar@ucw.cz>
 */

#undef LOCAL_DEBUG

#include <ucw/lib.h>
#include <ucw/conf.h>
#include <images/images.h>
#include <images/error.h>
#if defined(CONFIG_IMAGES_SIM) || defined(CONFIG_IMAGES_DUP)
#include <images/signature.h>
#endif

#include <string.h>

/* ImageLib section */
uint image_trace;
uint image_max_dim = 0xffff;
uint image_max_bytes = ~0U;

#if defined(CONFIG_IMAGES_SIM) || defined(CONFIG_IMAGES_DUP)
/* ImageSig section */
uint image_sig_min_width;
uint image_sig_min_height;
uint *image_sig_prequant_thresholds;
uint image_sig_postquant_min_steps;
uint image_sig_postquant_max_steps;
uint image_sig_postquant_threshold;
double image_sig_border_size;
int image_sig_border_bonus;
double image_sig_inertia_scale[3];
double image_sig_textured_threshold;
int image_sig_compare_method;
uint image_sig_cmp_features_weights[IMAGE_REG_F + IMAGE_REG_H];
#endif

static struct cf_section image_lib_config = {
  CF_ITEMS{
    CF_UINT("Trace", &image_trace),
    CF_UINT("ImageMaxDim", &image_max_dim),
    CF_UINT("ImageMaxBytes", &image_max_bytes),
    CF_END
  }
};

#if defined(CONFIG_IMAGES_SIM) || defined(CONFIG_IMAGES_DUP)
static struct cf_section image_sig_config = {
  CF_ITEMS{
    CF_UINT("MinWidth", &image_sig_min_width),
    CF_UINT("MinHeight", &image_sig_min_height),
    CF_UINT_DYN("PreQuantThresholds", &image_sig_prequant_thresholds, CF_ANY_NUM),
    CF_UINT("PostQuantMinSteps", &image_sig_postquant_min_steps),
    CF_UINT("PostQuantMaxSteps", &image_sig_postquant_max_steps),
    CF_UINT("PostQuantThreshold", &image_sig_postquant_threshold),
    CF_DOUBLE("BorderSize", &image_sig_border_size),
    CF_INT("BorderBonus", &image_sig_border_bonus),
    CF_DOUBLE_ARY("InertiaScale", image_sig_inertia_scale, 3),
    CF_DOUBLE("TexturedThreshold", &image_sig_textured_threshold),
    CF_LOOKUP("CompareMethod", &image_sig_compare_method, ((const char * const []){"integrated", "fuzzy", "average", NULL})),
    CF_UINT_ARY("CompareFeaturesWeights", image_sig_cmp_features_weights, IMAGE_REG_F + IMAGE_REG_H),
    CF_END
  }
};
#endif

static void CONSTRUCTOR
images_init_config(void)
{
  cf_declare_section("ImageLib", &image_lib_config, 0);
#if defined(CONFIG_IMAGES_SIM) || defined(CONFIG_IMAGES_DUP)
  cf_declare_section("ImageSig", &image_sig_config, 0);
#endif
}
