/* SPDX-License-Identifier: MIT */
/* GroovePuter experiment: the Felucca ANALOG adapter, compiled as C. Arduino passes -D flags to C++ only, so this file is
 * unconditional; the linker drops it from images that do not reference fa_* (the experimental voice is flag-gated). */
#include "felucca/fa_analog_impl.h"
