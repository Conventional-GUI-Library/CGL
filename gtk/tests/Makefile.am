include $(top_srcdir)/Makefile.decl

SUBDIRS =

AM_CPPFLAGS = \
	-I$(top_srcdir)			\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gtk		\
	-I$(top_srcdir)/gtk		\	
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	-DSRCDIR=\""$(abs_srcdir)"\"	\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

DEPS = \
	$(top_builddir)/gtk/libgtk-3.la

progs_ldadd = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(top_builddir)/gdk/libgdk-3.la	\
	$(GTK_DEP_LIBS)

noinst_PROGRAMS = $(TEST_PROGS)


TEST_PROGS			+= testing
testing_SOURCES			 = testing.c
testing_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= treemodel
treemodel_SOURCES		 = \
				treemodel.h \
				treemodel.c \
				liststore.c \
				treestore.c \
				filtermodel.c \
				sortmodel.c \
				modelrefcount.c \
				gtktreemodelrefcount.h \
				gtktreemodelrefcount.c
treemodel_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= treeview
treeview_SOURCES		 = treeview.c
treeview_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= treeview-scrolling
treeview_scrolling_SOURCES	 = treeview-scrolling.c
treeview_scrolling_LDADD	 = $(progs_ldadd) -lm

TEST_PROGS			+= recentmanager
recentmanager_SOURCES 		 = recentmanager.c
recentmanager_LDADD   		 = $(progs_ldadd)

TEST_PROGS			+= floating
floating_SOURCES		 = floating.c
floating_LDADD			 = $(progs_ldadd)

#TEST_PROGS			+= object
#object_SOURCES			 = object.c pixbuf-init.c
#object_LDADD			 = $(progs_ldadd)

# this doesn't work in make distcheck, since running
# on a naked X server creates slightly different event
# sequences than running on a normal desktop
# TEST_PROGS			+= crossingevents
#crossingevents_SOURCES		 = crossingevents.c
#crossingevents_LDADD		 = $(progs_ldadd)

# Should be ported to new API's
#TEST_PROGS			+= filechooser
#filechooser_SOURCES		 = filechooser.c pixbuf-init.c
#filechooser_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= builder
builder_SOURCES			 = builder.c
builder_LDADD			 = $(progs_ldadd)
builder_LDFLAGS			 = -export-dynamic

if OS_UNIX
#TEST_PROGS			+= defaultvalue
#defaultvalue_SOURCES		 = defaultvalue.c pixbuf-init.c
#defaultvalue_LDADD 		 = $(progs_ldadd)
endif

TEST_PROGS			+= textbuffer
textbuffer_SOURCES		 = textbuffer.c pixbuf-init.c
textbuffer_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= textiter
textiter_SOURCES		 = textiter.c
textiter_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= expander
expander_SOURCES		 = expander.c
expander_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= action
action_SOURCES			 = action.c
action_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= stylecontext
stylecontext_SOURCES		 = stylecontext.c
stylecontext_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= papersize
papersize_SOURCES		 = papersize.c
papersize_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= cellarea
cellarea_SOURCES		 = cellarea.c
cellarea_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= treepath
treepath_SOURCES		 = treepath.c
treepath_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= accessible
accessible_SOURCES		 = accessible.c
accessible_LDADD		 = $(progs_ldadd)

TEST_PROGS			+= entry
entry_SOURCES			 = entry.c
entry_LDADD			 = $(progs_ldadd)

TEST_PROGS			+= grid
grid_SOURCES			 = grid.c
grid_LDADD			 = $(progs_ldadd)

EXTRA_DIST +=				\
	file-chooser-test-dir/empty     \
	file-chooser-test-dir/text.txt

-include $(top_srcdir)/git.mk
