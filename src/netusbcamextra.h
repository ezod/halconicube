#ifndef __NETUSBCAMEXTRA_H__
#define __NETUSBCAMEXTRA_H__

#undef __BEGIN_DECLS
#undef __END_DECLS
#ifdef __cplusplus
# define __BEGIN_DECLS extern "C" {
# define __END_DECLS }
#else
# define __BEGIN_DECLS
# define __END_DECLS
#endif

__BEGIN_DECLS

typedef char bool;

enum {
    REG_BRIGHTNESS = 1,
    REG_CONTRAST = 2,
    REG_GAMMA = 3,
    REG_FLIPPED_V = 4,
    REG_FLIPPED_H = 5,
    REG_WHITE_BALANCE = 6,
    REG_EXPOSURE_TIME = 7,
    REG_EXPOSURE_TARGET = 8,
    REG_RED = 9,
    REG_GREEN = 10,
    REG_BLUE = 11,
    REG_BLACKLEVEL = 12,
    REG_GAIN = 13,
    REG_COLOR = 14,
    REG_PLL = 15,
    REG_STROBE_LENGTH = 16,
    REG_STROBE_DELAY = 17,
    REG_TRIGGER_DELAY = 18,
    REG_SATURATION = 19,
    REG_COLOR_MACHINE = 20,
    REG_TRIGGER_INVERT =  21,
    REG_MEASURE_FIELD_AE = 22,
    REG_SHUTTER = 26,
    REG_DEFECT_COR = 43
};

__END_DECLS

#endif /* __NETUSBCAMEXTRA_H__ */
