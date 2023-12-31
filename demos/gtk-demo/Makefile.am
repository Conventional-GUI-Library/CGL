## Makefile.am for gtk+/demos
include $(top_srcdir)/Makefile.decl

## These should be in the order you want them to appear in the
## demo app, which means alphabetized by demo title, not filename
demos =						\
	application.c				\
	appwindow.c				\
	assistant.c				\
	builder.c				\
	button_box.c				\
	changedisplay.c				\
	clipboard.c				\
	colorsel.c				\
	combobox.c				\
	css_accordion.c                         \
	css_basics.c				\
	css_multiplebgs.c			\
	css_pixbufs.c				\
	css_shadows.c				\
	dialog.c				\
	drawingarea.c				\
	editable_cells.c			\
	entry_buffer.c				\
	entry_completion.c			\
	event_axes.c				\
	expander.c  				\
	hypertext.c				\
	iconview.c				\
	iconview_edit.c				\
	images.c				\
	infobar.c				\
	links.c					\
	listbox.c				\
	flowbox.c				\
	list_store.c				\
	menus.c					\
	offscreen_window.c			\
	offscreen_window2.c			\
	overlay.c				\
	panes.c					\
	pickers.c				\
	pixbufs.c				\
	popover.c				\
	printing.c				\
	revealer.c				\
	rotated_text.c				\
	search_entry.c				\
	search_entry2.c				\
	sizegroup.c				\
	spinner.c				\
	stack.c					\
	stock_browser.c				\
	textview.c				\
	textscroll.c				\
	theming_style_classes.c			\
	toolpalette.c				\
	transparent.c				\
	tree_store.c				\
	ui_manager.c

AM_CPPFLAGS = \
	-I$(top_srcdir) 			\
	-I$(top_builddir)/gdk			\
	-DGDK_DISABLE_DEPRECATED		\
	-DGTK_DISABLE_DEPRECATED 		\
	$(GTK_DEBUG_FLAGS)			\
	$(GTK_DEP_CFLAGS)

DEPS = \
	$(top_builddir)/gtk/libgtk-3.la

LDADDS = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(top_builddir)/gdk/libgdk-3.la	\
	$(GTK_DEP_LIBS)				\
	-lm

bin_PROGRAMS = gtk3-demo gtk3-demo-application

desktopdir = $(datadir)/applications
dist_desktop_DATA = gtk3-demo.desktop

BUILT_SOURCES = demos.h demo_resources.c

EXTRA_DIST += 				\
	$(IMAGEFILES)			\
	demo.gresource.xml		\
	$(RESOURCES)			\
	org.gtk.Demo.gschema.xml

gsettings_SCHEMAS = \
	org.gtk.Demo.gschema.xml

@GSETTINGS_RULES@

demos.h: @REBUILD@ $(demos) geninclude.pl
	 $(AM_V_GEN) (here=`pwd` ; cd $(srcdir) && $(PERL) $$here/geninclude.pl $(demos)) > demos.h

gtk3_demo_SOURCES = 		\
	$(demos)		\
	demo_resources.c	\
	main.c			\
	demos.h

gtk3_demo_DEPENDENCIES = $(DEPS)
gtk3_demo_LDADD = $(LDADDS)
gtk3_demo_LDFLAGS = -export-dynamic

gtk3_demo_application_SOURCES = \
	application-standalone.c \
	demo_resources.c

gtk3_demo_application_LDADD = $(LDADDS)

demo_resources.c: demo.gresource.xml $(RESOURCES)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $(srcdir)/demo.gresource.xml

RESOURCES=	$(demos)			\
		application.ui			\
		demo.ui				\
		menus.ui			\
		stack.ui			\
		revealer.ui			\
		theming.ui			\
		listbox.ui			\
		alphatest.png			\
		apple-red.png			\
		background.jpg			\
		floppybuddy.gif			\
		gnome-applets.png		\
		gnome-calendar.png		\
		gnome-foot.png			\
		gnome-fs-directory.png		\
		gnome-fs-regular.png		\
		gnome-gimp.png			\
		gnome-gmush.png			\
		gnome-gsame.png			\
		gnu-keys.png			\
		gtk-logo-24.png			\
		gtk-logo-48.png			\
		gtk-logo-old.png		\
		messages.txt			\
		css_accordion.css		\
		css_basics.css			\
		css_multiplebgs.css		\
		css_pixbufs.css			\
		css_shadows.css			\
		cssview.css			\
		reset.css

iconthemedir = $(datadir)/icons/hicolor

appsicon16dir = $(iconthemedir)/16x16/apps
appsicon22dir = $(iconthemedir)/22x22/apps
appsicon24dir = $(iconthemedir)/24x24/apps
appsicon32dir = $(iconthemedir)/32x32/apps
appsicon48dir = $(iconthemedir)/48x48/apps
appsicon256dir = $(iconthemedir)/256x256/apps

dist_appsicon16_DATA = data/16x16/gtk3-demo.png
dist_appsicon22_DATA = data/22x22/gtk3-demo.png
dist_appsicon24_DATA = data/24x24/gtk3-demo.png
dist_appsicon32_DATA = data/32x32/gtk3-demo.png
dist_appsicon48_DATA = data/48x48/gtk3-demo.png
dist_appsicon256_DATA = data/256x256/gtk3-demo.png

update_icon_cache = gtk-update-icon-cache --ignore-theme-index --force

install-data-hook: install-update-icon-cache
uninstall-hook: uninstall-update-icon-cache

install-update-icon-cache:
	$(AM_V_at)$(POST_INSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

uninstall-update-icon-cache:
	$(AM_V_at)$(POST_UNINSTALL)
	test -n "$(DESTDIR)" || $(update_icon_cache) "$(iconthemedir)"

DISTCLEANFILES = demos.h

-include $(top_srcdir)/git.mk
