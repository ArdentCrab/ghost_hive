# Ghost Hive v1.7.1 — PSP-1004 (FAT) EBOOT
# Spec-Basis: §10, §12, §24
# Build: PSPSDK + `make` in this directory → EBOOT.PBP

TARGET = ghost_hive
OBJS = \
	main.o \
	registry.o \
	ghost_telemetry.o \
	event_queue.o \
	ghost_policy.o \
	context_engine.o \
	priority_engine.o \
	fallback_engine.o \
	replay_guard.o \
	decision_pipeline.o \
	ghost_heartbeat.o \
	ghost_scanner.o \
	ghost_stealth.o \
	ghost_peek.o \
	ghost_output.o \
	watch_hud.o \
	ghost_crypto.o \
	ghost_vault.o \
	ghost_keys.o \
	ghost_wrap.o \
	root_config.o \
	ghost_down.o \
	ghost_terminal.o \
	ghost_ir.o \
	psp_time.o \
	psp_input.o \
	hive_net.o \
	transport/ghost_transport.o \
	transport/medium_wlan.o \
	transport/medium_ir.o \
	transport/sensor_transport.o \
	transport/safe_transport.o \
	transport/mine_transport.o \
	transport/transport_frame.o \
	transport/transport_bidi.o \
	transport/worker_transport.o \
	../phone/sensor.o \
	../nas/safe.o \
	../mine/mine.o

INCDIR = . transport ../phone ../nas ../mine
CFLAGS = -O2 -G0 -Wall -include pspkernel.h -include psp_fw_decls.h
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti -std=c++11
ASFLAGS = $(CFLAGS)

LIBDIR =
# Adhoc/WLAN only. Do not prepend -lpspnet: build.mak appends SDK stubs last.
LIBS = -lstdc++ -lpsprtc -lpspwlan -lpspnet_adhoc -lpspnet_adhocctl -lpsppower

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Ghost Hive
PSP_FW_VERSION = 660
BUILD_PRX = 1
PSP_LARGE_MEMORY = 0

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
