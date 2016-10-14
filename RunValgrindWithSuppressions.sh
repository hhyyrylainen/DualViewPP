#!/bin/sh
G_DEBUG=resident-modules valgrind --tool=memcheck --leak-check=full --leak-resolution=high --num-callers=20 --log-file=vgdump --show-possibly-lost=no --suppressions=../GTK_Valgrind/base.supp --suppressions=../GTK_Valgrind/gdk.supp --suppressions=../GTK_Valgrind/glib.supp --suppressions=../GTK_Valgrind/gtksourceview.supp --suppressions=../GTK_Valgrind/pango.supp --suppressions=../GTK_Valgrind/gail.supp --suppressions=../GTK_Valgrind/gio.supp --suppressions=../GTK_Valgrind/gtk3.supp --suppressions=../GTK_Valgrind/gtk.supp ./dualviewpp
