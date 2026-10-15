#pragma once

#include <lvgl.h>

/* Fonts are declared per enabled screen. Multiple screens may be compiled in
 * at once (swipe navigation), so these are independent #if blocks; repeated
 * extern declarations of fonts shared between layouts are fine. */

#if defined(CONFIG_PROSPECTOR_SCREEN_CLASSIC_ENABLED)

LV_FONT_DECLARE(Symbols_Bold_26);
LV_FONT_DECLARE(Symbols_Regular_28);
LV_FONT_DECLARE(Symbols_Semibold_32);
LV_FONT_DECLARE(FG_Medium_20);
LV_FONT_DECLARE(FG_Medium_24);
LV_FONT_DECLARE(FR_Regular_48);
LV_FONT_DECLARE(FR_Thin_48);
LV_FONT_DECLARE(DINishCondensed_SemiBold_22);

#endif

#if defined(CONFIG_PROSPECTOR_SCREEN_RADII_ENABLED)

LV_FONT_DECLARE(Symbols_Semibold_32);
LV_FONT_DECLARE(Symbols_Semibold_28);
LV_FONT_DECLARE(Symbols_Medium_28);
LV_FONT_DECLARE(Symbols_Regular_28);
LV_FONT_DECLARE(Symbols_Bold_26);
LV_FONT_DECLARE(PPF_NarrowThin_64);
LV_FONT_DECLARE(DINishCondensed_SemiBold_22);

#endif

#if defined(CONFIG_PROSPECTOR_SCREEN_FIELD_ENABLED)

LV_FONT_DECLARE(Symbols_Semibold_32);
LV_FONT_DECLARE(Symbols_Regular_28);
LV_FONT_DECLARE(Symbols_Bold_26);
LV_FONT_DECLARE(FR_Regular_30);
LV_FONT_DECLARE(FR_Regular_36);
LV_FONT_DECLARE(FG_Medium_26);
LV_FONT_DECLARE(DINishCondensed_SemiBold_20);

#endif

#if defined(CONFIG_PROSPECTOR_SCREEN_OPERATOR_ENABLED)

LV_FONT_DECLARE(FG_Medium_20);
LV_FONT_DECLARE(FG_Medium_21);
LV_FONT_DECLARE(FG_Medium_26);
LV_FONT_DECLARE(DINishExpanded_Light_36);
LV_FONT_DECLARE(FR_Medium_32);
LV_FONT_DECLARE(DINish_Medium_24);

#endif

#if defined(CONFIG_PROSPECTOR_SCREEN_BONGO_ENABLED)

LV_FONT_DECLARE(FR_Medium_32);
LV_FONT_DECLARE(DINish_Medium_24);

#endif

#if defined(CONFIG_PROSPECTOR_SCREEN_POMODORO_ENABLED)

LV_FONT_DECLARE(PPF_NarrowThin_64);
LV_FONT_DECLARE(FG_Medium_20);

#endif
