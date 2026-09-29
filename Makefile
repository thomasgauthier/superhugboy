UNAME_S := $(shell uname -s)
target   := sdlarch
sources  := sdlarch.c glad.c

# Embedded resources: ROM + savestate compiled into the binary so that
# ./sdlarch runs standalone with no external game files. Edit these paths (or
# the files they point at) and rebuild to re-bake a new binary.
baked_rom     := roms/Super Mario World (USA).sfc
baked_state   := super-mario-world-first-boss.s9x
baked_level1  := level1.s9x
baked_castle  := super-mario-world-castle.s9x
baked_sources := build/baked_rom.c build/baked_state.c build/baked_level1.c build/baked_castle.c
baked_objects := build/baked_rom.o build/baked_state.o build/baked_level1.o build/baked_castle.o

CFLAGS   := -Wall -g
ifeq ($(UNAME_S),Darwin)
LFLAGS   := -static-libstdc++
else
LFLAGS   := -static-libgcc
endif
LIBS     := 
packages := sdl2

# do not edit from here onwards
objects := $(addprefix build/,$(sources:.c=.o)) $(baked_objects)
ifneq ($(packages),)
    LIBS    += $(shell pkg-config --libs-only-l $(packages))
    LFLAGS  += $(shell pkg-config --libs-only-L --libs-only-other $(packages))
    CFLAGS  += $(shell pkg-config --cflags $(packages))
endif

.PHONY: all clean FORCE

all: $(target)
FORCE:
clean:
	-rm -rf build
	-rm -f $(target)

$(target): Makefile $(objects)
	$(CC) $(LFLAGS) -o $@ $(objects) $(LIBS)

# Generate C arrays from the baked-in resource files (via xxd -i).
# "FORCE" re-runs these each build, but entangled with cmp so the object only
# recompiles when the underlying ROM/savestate actually changes.
build/baked_rom.c: FORCE
	@mkdir -p $(dir $@)
	@xxd -i -n baked_rom "$(baked_rom)" > $@.tmp
	@if cmp -s $@.tmp $@ 2>/dev/null; then rm -f $@.tmp; else mv $@.tmp $@; fi

build/baked_state.c: FORCE
	@mkdir -p $(dir $@)
	@xxd -i -n baked_state "$(baked_state)" > $@.tmp
	@if cmp -s $@.tmp $@ 2>/dev/null; then rm -f $@.tmp; else mv $@.tmp $@; fi

build/baked_level1.c: FORCE
	@mkdir -p $(dir $@)
	@xxd -i -n baked_level1 "$(baked_level1)" > $@.tmp
	@if cmp -s $@.tmp $@ 2>/dev/null; then rm -f $@.tmp; else mv $@.tmp $@; fi

build/baked_castle.c: FORCE
	@mkdir -p $(dir $@)
	@if [ -f "$(baked_castle)" ]; then \
	  xxd -i -n baked_castle "$(baked_castle)" > $@.tmp; \
	else \
	  printf 'unsigned char baked_castle[] = { 0 };\nunsigned int baked_castle_len = 0;\n' > $@.tmp; \
	fi
	@if cmp -s $@.tmp $@ 2>/dev/null; then rm -f $@.tmp; else mv $@.tmp $@; fi

build/%.o: %.c Makefile
	-@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -MMD -o $@ $<

-include $(addprefix build/,$(sources:.c=.d)) $(baked_sources:.c=.d)

