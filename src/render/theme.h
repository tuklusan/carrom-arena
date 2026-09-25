#ifndef CARROM_THEME_H
#define CARROM_THEME_H

#include <raylib.h>

/* Team colours. The rules code still calls the teams white and black (coin colour in the standard game); on screen the
 * white team is RED and the black team is BLUE. The queen is green so that she stays distinct from the red team. */
#define THEME_RED        (Color){ 214, 48, 48, 255 }
#define THEME_RED_LIGHT  (Color){ 240, 120, 108, 255 }
#define THEME_RED_DARK   (Color){ 128, 28, 30, 255 }
#define THEME_BLUE       (Color){ 44, 96, 220, 255 }
#define THEME_BLUE_LIGHT (Color){ 116, 160, 246, 255 }
#define THEME_BLUE_DARK  (Color){ 26, 52, 132, 255 }
#define THEME_QUEEN      (Color){ 52, 190, 96, 255 }
#define THEME_COIN_RIM   (Color){ 28, 28, 34, 210 }

#endif /* CARROM_THEME_H */
