#pragma once
#include "lvgl.h"

/* 0 = stub (text only splash), 1 = real bitmap (run tools/png2lvgl.py first) */
#define KNI_LOGO_VALID 1

extern const lv_image_dsc_t kni_logo;
