N := hazna
D := HAZNA

engine_csrcs := core
engine_libs := -lc41
engine_pub_hdrs := include/$(N).h
engine_priv_hdrs :=
engine_dl_opts := -ffreestanding -nostartfiles -nostdlib -Wl,-soname,lib$(N).so

cli_csrcs := cli test
clitool_libs := -lc41 -lhbs1clid -lhbs1

########
cflags := -fvisibility=hidden -Iinclude -Wall -Wextra -Werror $(cflags_ext)
cflags_sl := $(cflags) -D$(D)_STATIC
cflags_dl := $(cflags) -D$(D)_DLIB_BUILD -fpic
cflags_rls := -DNDEBUG
cflags_dbg := -D_DEBUG

engine_dl_rls_objs := $(patsubst %,out/engine-dl-rls/%.o,$(engine_csrcs))
engine_dl_dbg_objs := $(patsubst %,out/engine-dl-dbg/%.o,$(engine_csrcs))

clitool_dl_objs := $(patsubst %,out/cli-dl/%.o,$(cli_csrcs))

ifeq ($(PREFIX_DIR),)
PREFIX_DIR:=$(HOME)/.local
endif

.PHONY: all clean tags arc engines engine-dl-rls engine-dl-dbg clitools cli-dl install uninstall test

all: engines clitools

arc:
	cd .. && tar -Jcvf $(N).txz $(N)/src $(N)/include $(N)/make* $(N)/README* $(N)/LICENCE $(N)/*.cmd

install: engine-dl-rls cli-dl
	mkdir -p $(PREFIX_DIR)/bin
	mkdir -p $(PREFIX_DIR)/lib
	mkdir -p $(PREFIX_DIR)/include
	cp -v out/engine-dl-rls/lib$(N).so $(PREFIX_DIR)/lib/
	[ `whoami` != root ] || ldconfig
	cp -v out/cli-dl/$(N) $(PREFIX_DIR)/bin/$(N)
	cp -vr include/$(N).h $(PREFIX_DIR)/include/$(N).h

uninstall:
	-rm -f $(PREFIX_DIR)/lib/lib$(N).so
	-rm -rf $(PREFIX_DIR)/include/$(N).h
	[ `whoami` != root ] || ldconfig

clean:
	-rm -rf out tags

tags:
	ctags -R --fields=+iaS --extra=+q --exclude='.git' .

engines: engine-dl-rls engine-dl-dbg

clitools: cli-dl

test: engines cli-dl
	LD_LIBRARY_PATH=out/engine-dl-dbg:$(LD_LIBRARY_PATH) out/cli-dl/$(N) test
	LD_LIBRARY_PATH=out/engine-dl-rls:$(LD_LIBRARY_PATH) out/cli-dl/$(N) test

engine-dl-rls: out/engine-dl-rls/lib$(N).so

engine-dl-dbg: out/engine-dl-dbg/lib$(N).so

cli-dl: out/cli-dl/$(N)

# dirs
out out/engine-dl-rls out/engine-dl-dbg out/cli-dl:
	mkdir -p $@

# dynamic libs
out/engine-dl-rls/lib$(N).so: $(engine_dl_rls_objs) | out/engine-dl-rls
	gcc -shared	-o$@ $(engine_dl_opts) $(cflags_dl) $^ $(engine_libs)

out/engine-dl-dbg/lib$(N).so: $(engine_dl_dbg_objs) | out/engine-dl-dbg
	gcc -shared	-o$@ $(engine_dl_opts) $(cflags_dl) $^ $(engine_libs)

# object files for dynamic release lib
$(engine_dl_rls_objs): out/engine-dl-rls/%.o: src/%.c $(engine_pub_hdrs) $(engine_priv_hdrs) | out/engine-dl-rls
	gcc -c -o$@ $< -Iinclude $(cflags_dl) $(cflags_rls)

# object files for dynamic debug lib
$(engine_dl_dbg_objs): out/engine-dl-dbg/%.o: src/%.c $(engine_pub_hdrs) $(engine_priv_hdrs) | out/engine-dl-dbg
	gcc -c -o$@ $< -Iinclude $(cflags_dl) $(cflags_dbg)

# command line tool (dynamic linked)
out/cli-dl/$(N): $(patsubst %,src/%.c,$(cli_csrcs)) $(engine_pub_hdrs) out/engine-dl-rls/lib$(N).so | out/cli-dl
	gcc -o$@ $(patsubst %,src/%.c,$(cli_csrcs)) $(cflags) -Lout/engine-dl-rls -l$(N) $(clitool_libs)


