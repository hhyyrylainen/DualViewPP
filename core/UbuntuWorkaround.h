#pragma once
#include <gtk/gtk.h>

#if GTK_MINOR_VERSION >= 20

// No need to use workaround
#undef DV_BUILDER_WORKAROUND

#else

// Workaround for old gtk versions
#define DV_BUILDER_WORKAROUND 1

#endif


