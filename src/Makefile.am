SUBDIRS = ../.mingw bled ext2fs ms-sys syslinux/libfat syslinux/libinstaller syslinux/win libcdio/iso9660 libcdio/udf libcdio/driver wimlib ../res/loc
# As far as I can tell, the following libraries are *not* vulnerable to side-loading, so we link using their regular version:
NONVULNERABLE_LIBS = -lole32 -lgdi32 -lshlwapi -lcomctl32 -luuid -lntdll
# The following libraries are vulnerable (or have an unknown vulnerability status), so we link using our delay-loaded replacement:
# See https://github.com/pbatard/rufus/issues/2272
VULNERABLE_LIBS = -lcrypt32-delaylib -ldwmapi-delaylib -lsetupapi-delaylib -luxtheme-delaylib -lversion-delaylib -lvirtdisk-delaylib -lwininet-delaylib -lwintrust-delaylib

noinst_PROGRAMS = rufus

AM_V_WINDRES_0 = @echo "  RC     $@";$(WINDRES)
AM_V_WINDRES_1 = $(WINDRES)
AM_V_WINDRES_  = $(AM_V_WINDRES_$(AM_DEFAULT_VERBOSITY))
AM_V_WINDRES   = $(AM_V_WINDRES_$(V))

%_rc.o: %.rc ../res/loc/embedded.loc
	$(AM_V_WINDRES) $(AM_RCFLAGS) -i $< -o $@

rufus_SOURCES = badblocks.c darkmode.c dev.c dos.c dos_locale.c drive.c format.c format_ext.c format_fat32.c hash.c icon.c iso.c localization.c \
	 net.c parser.c pki.c process.c cregex_compile.c cregex_parse.c cregex_vm.c rufus.c smart.c stdfn.c stdio.c stdlg.c syslinux.c ui.c vhd.c wue.c xml.c
rufus_CFLAGS = -I$(srcdir)/ms-sys/inc -I$(srcdir)/syslinux/libfat -I$(srcdir)/syslinux/libinstaller -I$(srcdir)/syslinux/win -I$(srcdir)/libcdio -I$(srcdir)/wimlib -I$(srcdir)/../res $(AM_CFLAGS) \
	-DEXT2_FLAT_INCLUDES=0 -D_RUFUS -DSOLUTION=rufus
rufus_LDFLAGS = $(AM_LDFLAGS) -mwindows -L../.mingw
rufus_LDADD = $(zig_hash_libs_$(ZIG_HASH)) rufus_rc.o bled/libbled.a ext2fs/libext2fs.a ms-sys/libmssys.a syslinux/libfat/libfat.a syslinux/libinstaller/libinstaller.a syslinux/win/libwin.a \
	libcdio/iso9660/libiso9660.a libcdio/udf/libudf.a libcdio/driver/libdriver.a wimlib/libwim.a $(NONVULNERABLE_LIBS) $(VULNERABLE_LIBS)

# Opt-in Zig backend: make ZIG_HASH=1 ZIG=/path/to/zig
ZIG_HASH ?= 0
ZIG ?= zig
ZIG_CC_TARGET := $(shell $(CC) -dumpmachine)
ZIG_TARGET := $(if $(filter x86_64-%,$(ZIG_CC_TARGET)),x86_64-windows-gnu,$(if $(filter i386-% i486-% i586-% i686-%,$(ZIG_CC_TARGET)),x86-windows-gnu,$(if $(filter aarch64-%,$(ZIG_CC_TARGET)),aarch64-windows-gnu)))
zig_hash_cflags_1 = -DRUFUS_USE_ZIG_HASH
zig_hash_libs_1 = zig/librufushash.a
rufus_CFLAGS += $(zig_hash_cflags_$(ZIG_HASH))
EXTRA_rufus_DEPENDENCIES = $(zig_hash_libs_$(ZIG_HASH))

.PHONY: zig-hash-toolchain
zig-hash-toolchain:
	@case "$$($(ZIG) version)" in 0.16.0|0.17.0-dev.2294+71403f299) ;; *) echo "Use a Zig version pinned in docs/zig-toolchain.json"; exit 1 ;; esac
	@test -n "$(ZIG_TARGET)" -a -n "$(findstring mingw,$(ZIG_CC_TARGET))" || { echo "Unsupported Zig hash compiler target: $(ZIG_CC_TARGET)"; exit 1; }

zig/librufushash.a: $(srcdir)/zig/hash.zig zig-hash-toolchain
	$(AM_V_GEN)$(MKDIR_P) zig
	$(AM_V_GEN)$(ZIG) build-lib $(srcdir)/zig/hash.zig -target $(ZIG_TARGET) -O ReleaseSafe -fcompiler-rt -femit-bin=$@

# Recompile the C adapter when switching backends without requiring make clean.
.PHONY: zig-hash-mode-check
zig-hash-mode-check:

zig/hash-mode: zig-hash-mode-check
	@$(MKDIR_P) zig
	@echo "$(ZIG_HASH) $(ZIG_CC_TARGET)" > zig/hash-mode.tmp
	@cmp -s zig/hash-mode.tmp $@ || cp zig/hash-mode.tmp $@
	@$(RM) zig/hash-mode.tmp

rufus-hash.$(OBJEXT): zig/hash-mode

clean-local:
	$(RM) zig/librufushash.a zig/hash-mode zig/hash-mode.tmp

EXTRA_DIST = zig/hash.zig zig/hash.h
