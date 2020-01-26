bin_PROGRAMS    = jush
jush_SOURCES    = jush.c jush.h builtin.c complete.c expand.c jobs.c queue.h recompose.c
jush_CFLAGS     = -W -Wall -Wextra -Wno-unused-parameter -std=gnu99
jush_CFLAGS    += $(editline_CFLAGS)
jush_LDADD      = $(editline_LIBS) $(LIBOBJS)
