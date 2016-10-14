#!/bin/sh
# G_SLICE=debug-blocks this seems to cause invalid writes
# --gen-suppressions=all
G_DEBUG=resident-modules valgrind --tool=memcheck --show-possibly-lost=no --leak-check=full --leak-resolution=high --num-callers=20 --log-file=vgdump --suppressions=../GTK_Valgrind/base.supp --suppressions=../GTK_Valgrind/gdk.supp --suppressions=../GTK_Valgrind/glib.supp --suppressions=../GTK_Valgrind/gtksourceview.supp --suppressions=../GTK_Valgrind/pango.supp --suppressions=../GTK_Valgrind/gail.supp --suppressions=../GTK_Valgrind/gio.supp --suppressions=../GTK_Valgrind/gtk3.supp --suppressions=../GTK_Valgrind/gtk.supp --suppressions=../GTK_Valgrind/gtk.suppression --suppressions=../GTK_Valgrind/own.supp ./dualviewpp
