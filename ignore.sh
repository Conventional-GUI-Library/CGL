#!/bin/sh

# the purpose of this script is to ask git to ignore future changes to files
# which get altered by the build system.

set -- "INSTALL" \
    "config.h" \
    "gdk/gdkenumtypes.h" \
    "gtk/gtkbuiltincache.h" \
    "gtk/gtkmarshalers.h" \
    "gtk/gtktypebuiltins.c" \
    "gtk/gtktypebuiltins.h" \
    "gtk/gtktypefuncs.c" \
    "gtk/stock-icons/icon-theme.cache" \
    "gtk/stock-icons/16/document-save.png" \
    "gtk/stock-icons/16/folder-remote.png" \
    "gtk/stock-icons/16/go-first-rtl.png" \
    "gtk/stock-icons/16/go-last-rtl.png" \
    "gtk/stock-icons/16/go-next-rtl.png" \
    "gtk/stock-icons/16/go-previous-rtl.png" \
    "gtk/stock-icons/16/media-seek-backward-rtl.png" \
    "gtk/stock-icons/16/media-seek-forward-rtl.png" \
    "gtk/stock-icons/16/media-skip-backward-rtl.png" \
    "gtk/stock-icons/16/media-skip-forward-rtl.png" \
    "gtk/stock-icons/16/user-desktop.png" \
    "gtk/stock-icons/16/user-home.png" \
    "gtk/stock-icons/24/document-save.png" \
    "gtk/stock-icons/24/folder-remote.png" \
    "gtk/stock-icons/24/go-first-rtl.png" \
    "gtk/stock-icons/24/go-last-rtl.png" \
    "gtk/stock-icons/24/go-next-rtl.png" \
    "gtk/stock-icons/24/go-previous-rtl.png" \
    "gtk/stock-icons/24/media-seek-backward-rtl.png" \
    "gtk/stock-icons/24/media-seek-forward-rtl.png" \
    "gtk/stock-icons/24/media-skip-backward-rtl.png" \
    "gtk/stock-icons/24/media-skip-forward-rtl.png" \
    "gtk/stock-icons/24/user-desktop.png" \
    "gtk/stock-icons/24/user-home.png" \
    "aclocal.m4" \
    "m4/gtk-doc.m4" \
    "gtk-doc.make" \
    "Makefile.in" \
    "build-aux/compile" \
    "build-aux/config.guess" \
    "build-aux/config.sub" \
    "build-aux/depcomp" \
    "build-aux/install-sh" \
    "build-aux/ltmain.sh" \
    "build-aux/missing" \
    "build-aux/test-driver" \
    "build/Makefile.in" \
    "build/win32/Makefile.in" \
    "build/win32/vs9/Makefile.in" \
    "config.h.in" \
    "config.h.win32" \
    "docs/Makefile.in" \
    "docs/reference/Makefile.in" \
    "docs/reference/gdk/Makefile.in" \
    "docs/reference/gdk/version.xml" \
    "docs/reference/gtk/Makefile.in" \
    "docs/reference/gtk/gtk3.types" \
    "docs/reference/gtk/version.xml" \
    "docs/reference/libgail-util/Makefile.in" \
    "docs/reference/libgail-util/version.xml" \
    "docs/tools/Makefile.in" \
    "examples/Makefile.in" \
    "gdk/Makefile.in" \
    "gdk/broadway/Makefile.in" \
    "gdk/quartz/Makefile.in" \
    "gdk/tests/Makefile.in" \
    "gdk/wayland/Makefile.in" \
    "gdk/win32/Makefile.in" \
    "gdk/win32/rc/Makefile.in" \
    "gdk/win32/rc/gdk.rc" \
    "gdk/x11/Makefile.in" \
    "gtk/Makefile.in" \
    "gtk/gtk-win32.rc" \
    "gtk/gtkversion.h" \
    "gtk/makefile.msc" \
    "gtk/tests/Makefile.in" \
    "libgail-util/Makefile.in" \
    "m4/libtool.m4" \
    "m4/ltoptions.m4" \
    "m4/ltsugar.m4" \
    "m4/ltversion.m4" \
    "m4/lt~obsolete.m4" \
    "m4macros/Makefile.in" \
    "modules/Makefile.in" \
    "modules/engines/Makefile.in" \
    "modules/engines/ms-windows/Makefile.in" \
    "modules/engines/ms-windows/Theme/Makefile.in" \
    "modules/engines/ms-windows/Theme/gtk-3.0/Makefile.in" \
    "modules/engines/pixbuf/Makefile.in" \
    "modules/input/Makefile.in" \
    "modules/printbackends/Makefile.in" \
    "modules/printbackends/cups/Makefile.in" \
    "modules/printbackends/file/Makefile.in" \
    "modules/printbackends/lpr/Makefile.in" \
    "modules/printbackends/papi/Makefile.in" \
    "modules/printbackends/test/Makefile.in" \
    "po-properties/Makefile.in" \
    "po-properties/POTFILES" \
    "po/Makefile.in" \
    "po/POTFILES" \
    "tests/Makefile.in" \
    "tests/a11y/Makefile.in" \
    "tests/css/Makefile.in" \
    "tests/css/parser/Makefile.in" \
    "tests/reftests/Makefile.in"

for file in $@; do
    if [ ! -e $file ]; then
        touch $file
    fi
    git update-index --skip-worktree "$file"
done
