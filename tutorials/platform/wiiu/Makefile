# This is a *really* silly makefile, but Nintendo's makes don't work
# that well if you're not in their directory tree, so until Multi is a
# viable project distibution, we'll go with this.

GRANNY_INC   = ../../../include
GRANNY_LIB   = ../../../lib/wiiu/granny2.a
PIXO_RUN     = media/pixo_run.gr2

CAFE_WIN_ROOT := $(shell cygpath -am $(CAFE_ROOT))
GHS_WIN_ROOT  := $(shell cygpath -aw $(GHS_ROOT))

CAFE_CC      = $(GHS_ROOT)/ccppc.exe
CAFE_LINK    = $(GHS_ROOT)/cxppc.exe
CAFE_PREPRPL = $(CAFE_ROOT)/system/bin/tool/preprpl64.exe
CAFE_MAKERPL = $(CAFE_ROOT)/system/bin/tool/makerpl64.exe

CAFE_LIBLIST = coredyn.a pad.a gfd.a mtx.a demo.a wbc.a wpad.a lint.a gx2.a bti.a avm.a tcl.a tve.a dc.a nn_save.a proc_ui.a gx2ut.a
CAFE_LIBLOC  = $(CAFE_WIN_ROOT)/system/lib/ghs/cafe/DEBUG
CAFE_LIBS    = $(patsubst %,$(CAFE_LIBLOC)/%,$(CAFE_LIBLIST))

CAFE_LINKFLAGS = -e _start -lnk=-nosegments_always_executable -nostartfile --no_exceptions \
                 --g++ --link_once_templates -relprog_cafe -cpu=espresso -warn_dbo_not_found_max=0 \
                 -sda=none -Ogeneral -G -L $(CAFE_LIBLOC) -Mn -Mu -map=out/wiiu_example.map -e _start -T

CAFE_CFLAGS = -D_DEBUG -DNDEV=1 -DCAFE=2 -DPLATFORM=CAFE -DEPPC -DCOS_SECURITY_ENABLED=1 -DIOS_DEBUG_STARTUP_STATE=FALSE \
              --no_commons -c99 -kanji=shiftjis -X332 --no_exceptions --g++ --link_once_templates -cpu=espresso \
              -warn_dbo_not_found_max=0 -sda=none -Odebug -G --quit_after_warnings \
              -dbg_source_root $(CAFE_WIN_ROOT)/system

CAFE_INCDIR = -I. -I $(GRANNY_INC) -I $(CAFE_WIN_ROOT)/system/include 


default: out/wiiu_example.rpx
	@echo Made $@ \"make run\" or \"make debugger\" to execute

clean:
	@rm -rf out

out/pixo_run.gsh: shaders/Fragment.ps shaders/Skinned.vs
	@echo Compiling shader
	@gshCompile -v shaders/Skinned.vs -p shaders/Fragment.ps -o $@
	@chmod 777 $@

run: out/wiiu_example.rpx out/pixo_run.gsh
	@cp -f $(PIXO_RUN) $(shell cygpath -am $(CAFE_CONTENT_DIR))
	@cp -f out/pixo_run.gsh $(shell cygpath -am $(CAFE_CONTENT_DIR))
	@caferun out/wiiu_example.rpx

debugger: out/wiiu_example.rpx out/pixo_run.gsh
	@cp -f $(PIXO_RUN) $(shell cygpath -am $(CAFE_CONTENT_DIR))
	@cp -f out/pixo_run.gsh $(shell cygpath -am $(CAFE_CONTENT_DIR))
	@caferun -g 1 -d multi out/wiiu_example.rpx 

out/wiiu_example.rpx: out/wiiu_example.elf
	@$(CAFE_MAKERPL) $^ -f -l wiiu_example.a

out/wiiu_example.elf: out/wiiu_example_rpl_export.o out/wiiu_example.o
	@$(CAFE_LINK) $^ $(CAFE_LIBS) $(GRANNY_LIB) $(CAFE_LINKFLAGS) $(CAFE_WIN_ROOT)/system/include/cafe/eppc.Cafe.ld -o $@

out/wiiu_example_rpl_export.o: out/wiiu_example.o
	@$(CAFE_PREPRPL) -e _start  -o $@ $^ $(CAFE_LIBS) $(GRANNY_LIB)

out/wiiu_example.o: wiiu_example.cpp
	@mkdir -p out
	@echo Compiling $^
	@$(CAFE_CC)  -c $^ -o $@ $(CAFE_CFLAGS) $(CAFE_INCDIR)

.PHONY: clean run debugger

