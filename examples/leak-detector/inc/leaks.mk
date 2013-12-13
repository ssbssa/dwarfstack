
LEAKS_FUNC=malloc \
	   calloc \
	   free \
	   realloc \
	   strdup \
	   wcsdup \
	   __main \

L_EMPTY=
,=,
$(L_EMPTY) $(L_EMPTY)=$(L_EMPTY) $(L_EMPTY)

LEAKS_CFLAGS=-fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-builtin
LEAKS_LDFLAGS=-Wl,$(subst $( ),$(,),$(patsubst %,--wrap$(,)%,$(LEAKS_FUNC)))
