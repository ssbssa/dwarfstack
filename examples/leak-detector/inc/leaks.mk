
LEAKS_FUNC=malloc \
	   calloc \
	   free \
	   realloc \
	   strdup \
	   wcsdup \
	   __main \

L_EMPTY=
,=,
SPACE=$(L_EMPTY) $(L_EMPTY)

LEAKS_CFLAGS=-fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-schedule-insns2 -fno-builtin
LEAKS_LDFLAGS=-Wl,$(subst $(SPACE),$(,),$(patsubst %,--wrap$(,)%,$(LEAKS_FUNC)))
