# Ghost Hive v1.7.1
# Host tests + kernel. PSP EBOOT: `make eboot` (needs PSPSDK).

INC = -I src/psp
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra $(INC)
PSP = src/psp
T = tests

PSP_CORE = \
	$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp \
	$(PSP)/event_queue.cpp \
	$(PSP)/ghost_policy.cpp \
	$(PSP)/context_engine.cpp \
	$(PSP)/priority_engine.cpp \
	$(PSP)/fallback_engine.cpp \
	$(PSP)/replay_guard.cpp \
	$(PSP)/decision_pipeline.cpp \
	$(PSP)/ghost_heartbeat.cpp \
	$(PSP)/ghost_scanner.cpp \
	$(PSP)/ghost_stealth.cpp \
	$(PSP)/ghost_peek.cpp \
	$(PSP)/ghost_output.cpp \
	$(PSP)/watch_hud.cpp \
	$(PSP)/ghost_crypto.cpp \
	$(PSP)/ghost_vault.cpp \
	$(PSP)/ghost_keys.cpp \
	$(PSP)/ghost_wrap.cpp \
	$(PSP)/root_config.cpp \
	$(PSP)/ghost_down.cpp \
	$(PSP)/ghost_terminal.cpp \
	$(PSP)/ghost_ir.cpp \
	$(PSP)/psp_time.cpp \
	$(PSP)/psp_input.cpp \
	$(PSP)/hive_net.cpp

PSP_XPORT = \
	$(PSP)/transport/ghost_transport.cpp \
	$(PSP)/transport/medium_wlan.cpp \
	$(PSP)/transport/medium_ir.cpp \
	$(PSP)/transport/sensor_transport.cpp \
	$(PSP)/transport/safe_transport.cpp \
	$(PSP)/transport/mine_transport.cpp \
	$(PSP)/transport/transport_frame.cpp \
	$(PSP)/transport/transport_bidi.cpp \
	$(PSP)/transport/worker_transport.cpp

PEERS = src/phone/sensor.cpp src/nas/safe.cpp src/mine/mine.cpp src/laptop/worker.cpp

LAPTOP_MODS = \
	src/laptop/worker.cpp \
	src/laptop/analyzer.cpp \
	src/laptop/log_client.cpp \
	src/laptop/alert.cpp \
	src/laptop/kill.cpp \
	src/laptop/peer_keys.cpp \
	src/laptop/peer_halt.cpp \
	src/laptop/tetact.cpp \
	src/laptop/host_telem.cpp

PEER_INC = -I src/psp -I src/laptop -I src/phone -I src/nas -I src/mine -I src/router

PEER_XPORT = \
	$(PSP)/event_queue.cpp \
	$(PSP)/transport/medium_wlan.cpp \
	$(PSP)/transport/medium_ir.cpp \
	$(PSP)/transport/sensor_transport.cpp \
	$(PSP)/transport/safe_transport.cpp \
	$(PSP)/transport/mine_transport.cpp \
	$(PSP)/transport/transport_frame.cpp \
	$(PSP)/transport/transport_bidi.cpp \
	$(PSP)/transport/worker_transport.cpp \
	$(PSP)/ghost_ir.cpp \
	$(PSP)/psp_time.cpp

.PHONY: test host mini eboot peers pack-live lab lab-invariants lab-sim lab-engines lab-fuzz lab-smoke lab-test-phase lab-24h redteam

test: export GHOST_DOWN_ARMED=1
test:
	$(CXX) $(CXXFLAGS) $(T)/test_registry.cpp $(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp -o /tmp/t1 && /tmp/t1
	$(CXX) $(CXXFLAGS) $(T)/test_event_queue.cpp $(PSP)/event_queue.cpp -o /tmp/t2 && /tmp/t2
	$(CXX) $(CXXFLAGS) $(T)/test_policy.cpp $(PSP)/ghost_policy.cpp -o /tmp/t3 && /tmp/t3
	$(CXX) $(CXXFLAGS) $(T)/test_context_priority.cpp $(PSP)/context_engine.cpp $(PSP)/priority_engine.cpp -o /tmp/t4 && /tmp/t4
	$(CXX) $(CXXFLAGS) $(T)/test_replay_guard.cpp $(PSP)/replay_guard.cpp $(PSP)/ghost_crypto.cpp $(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp -o /tmp/t5 && /tmp/t5
	$(CXX) $(CXXFLAGS) $(T)/test_spec.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/context_engine.cpp $(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp \
		$(PSP)/replay_guard.cpp $(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp \
		$(PSP)/ghost_scanner.cpp $(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp \
		$(PSP)/ghost_output.cpp $(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp \
		$(PSP)/ghost_keys.cpp $(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp \
		src/mine/mine.cpp $(PSP_XPORT) -o /tmp/t6 && /tmp/t6
	$(CXX) $(CXXFLAGS) $(T)/test_p36.cpp $(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp $(PEERS) $(PSP_XPORT) -o /tmp/t7 && /tmp/t7
	$(CXX) $(CXXFLAGS) -I src/laptop -I src/mine -I src/phone $(T)/test_p30_mitm.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp src/mine/mine.cpp \
		src/laptop/alert.cpp src/laptop/kill.cpp src/laptop/peer_keys.cpp \
		$(PSP_XPORT) -o /tmp/t8 && /tmp/t8
	$(CXX) $(CXXFLAGS) -I src/laptop -I src/mine -I src/phone $(T)/test_p30_replay.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp src/mine/mine.cpp \
		src/laptop/alert.cpp src/laptop/kill.cpp src/laptop/peer_keys.cpp \
		$(PSP_XPORT) -o /tmp/t9 && /tmp/t9
	$(CXX) $(CXXFLAGS) -I src/laptop -I src/mine -I src/phone $(T)/test_p30_worker.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp src/mine/mine.cpp \
		src/laptop/alert.cpp src/laptop/kill.cpp src/laptop/peer_keys.cpp \
		$(PSP_XPORT) -o /tmp/t10 && /tmp/t10
	$(CXX) $(CXXFLAGS) -I src/laptop -I src/mine -I src/phone $(T)/test_p30_stick.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp src/mine/mine.cpp \
		src/laptop/alert.cpp src/laptop/kill.cpp src/laptop/peer_keys.cpp \
		$(PSP_XPORT) -o /tmp/t11 && /tmp/t11
	$(CXX) $(CXXFLAGS) -I src/laptop -I src/mine -I src/phone $(T)/test_p30_vault.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp src/mine/mine.cpp \
		$(PSP)/decision_pipeline.cpp \
		src/laptop/alert.cpp src/laptop/kill.cpp src/laptop/peer_keys.cpp \
		$(PSP_XPORT) -o /tmp/t12 && /tmp/t12
	$(CXX) $(CXXFLAGS) -I src/laptop -I src/mine -I src/phone $(T)/test_p30_time.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp src/mine/mine.cpp \
		src/laptop/alert.cpp src/laptop/kill.cpp src/laptop/peer_keys.cpp \
		$(PSP_XPORT) -o /tmp/t13 && /tmp/t13
	$(CXX) $(CXXFLAGS) -I src/laptop -I src/mine -I src/phone $(T)/test_p30_kill.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp src/mine/mine.cpp \
		src/laptop/alert.cpp src/laptop/kill.cpp src/laptop/peer_keys.cpp \
		src/laptop/peer_halt.cpp \
		$(PSP_XPORT) -o /tmp/t14 && /tmp/t14
	$(CXX) $(CXXFLAGS) -I src/laptop -I src/mine -I src/phone $(T)/test_p30_route.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp src/mine/mine.cpp \
		src/laptop/alert.cpp src/laptop/kill.cpp src/laptop/peer_keys.cpp \
		$(PSP_XPORT) -o /tmp/t15 && /tmp/t15
	$(CXX) $(CXXFLAGS) -I src/laptop -I src/mine -I src/phone $(T)/test_root_config.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/root_config.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_crypto.cpp -o /tmp/t16 && /tmp/t16
	$(CXX) $(CXXFLAGS) $(T)/test_root_wrap.cpp $(PSP)/ghost_wrap.cpp $(PSP)/ghost_crypto.cpp \
		-o /tmp/t17 && /tmp/t17
	$(CXX) $(CXXFLAGS) -I src/laptop $(T)/test_tetact.cpp src/laptop/tetact.cpp \
		-o /tmp/t18 && /tmp/t18
	$(CXX) $(CXXFLAGS) -I src/laptop -I src/mine -I src/phone $(T)/test_phase_c.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_wrap.cpp $(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp \
		src/mine/mine.cpp src/mine/os_mine.cpp src/mine/browser_mine.cpp \
		src/laptop/alert.cpp src/laptop/kill.cpp src/laptop/peer_keys.cpp \
		src/laptop/peer_halt.cpp \
		src/laptop/tetact.cpp \
		$(PSP_XPORT) -o /tmp/t19 && /tmp/t19
	$(CXX) $(CXXFLAGS) -I src/psp -I src/laptop $(T)/test_peer_halt.cpp \
		src/laptop/peer_halt.cpp src/laptop/peer_keys.cpp \
		$(PSP)/ghost_keys.cpp $(PSP)/ghost_crypto.cpp -o /tmp/t20 && /tmp/t20
	$(CXX) $(CXXFLAGS) -I src/laptop -I src/mine -I src/phone $(T)/test_phase_d.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_wrap.cpp $(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp \
		src/mine/mine.cpp src/mine/os_mine.cpp src/mine/browser_mine.cpp \
		src/laptop/alert.cpp src/laptop/kill.cpp src/laptop/peer_keys.cpp \
		src/laptop/peer_halt.cpp \
		$(PSP_XPORT) -o /tmp/t21 && /tmp/t21
	$(CXX) $(CXXFLAGS) -I src/laptop -I src/mine -I src/phone $(T)/test_phase_e.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_wrap.cpp $(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp \
		src/mine/mine.cpp \
		src/laptop/alert.cpp src/laptop/kill.cpp src/laptop/peer_keys.cpp \
		$(PSP_XPORT) -o /tmp/t22 && /tmp/t22
	$(CXX) $(CXXFLAGS) -DGHOST_DEBUG_CLI=1 $(T)/test_controls.cpp \
		$(PSP)/ghost_terminal.cpp $(PSP)/psp_input.cpp $(PSP)/psp_time.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/watch_hud.cpp $(PSP)/hive_net.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp $(PSP_XPORT) \
		-o /tmp/t23 && /tmp/t23
	$(CXX) $(CXXFLAGS) $(T)/test_watch.cpp \
		$(PSP)/ghost_terminal.cpp $(PSP)/psp_input.cpp $(PSP)/psp_time.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/watch_hud.cpp $(PSP)/hive_net.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp $(PSP_XPORT) \
		-o /tmp/t24 && /tmp/t24
	$(CXX) $(CXXFLAGS) $(T)/test_alarm_latency.cpp \
		$(PSP)/ghost_terminal.cpp $(PSP)/psp_input.cpp $(PSP)/psp_time.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/watch_hud.cpp $(PSP)/hive_net.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp $(PSP_XPORT) \
		-o /tmp/t24a && /tmp/t24a
	$(CXX) $(CXXFLAGS) $(PEER_INC) $(T)/test_opsec.cpp src/laptop/peer_keys.cpp \
		$(PSP)/ghost_keys.cpp $(PSP)/ghost_crypto.cpp -o /tmp/t24b && /tmp/t24b
	$(CXX) $(CXXFLAGS) $(T)/test_telemetry.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp \
		$(PSP)/ghost_policy.cpp $(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp \
		$(PSP)/replay_guard.cpp $(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp \
		$(PSP)/ghost_scanner.cpp $(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp \
		$(PSP)/ghost_output.cpp $(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp \
		$(PSP)/ghost_keys.cpp $(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp \
		$(PSP_XPORT) -o /tmp/t25 && /tmp/t25
	$(CXX) $(CXXFLAGS) -I src/laptop -I src/mine -I src/phone $(T)/test_security.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp src/mine/mine.cpp \
		src/laptop/alert.cpp src/laptop/kill.cpp src/laptop/peer_keys.cpp \
		$(PSP_XPORT) -o /tmp/t26 && /tmp/t26
	$(CXX) $(CXXFLAGS) $(T)/test_perf.cpp \
		$(PSP)/ghost_terminal.cpp $(PSP)/psp_input.cpp $(PSP)/psp_time.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/watch_hud.cpp $(PSP)/hive_net.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp $(PSP_XPORT) \
		-o /tmp/t27 && /tmp/t27
	$(CXX) $(CXXFLAGS) $(T)/test_ui.cpp \
		$(PSP)/ghost_terminal.cpp $(PSP)/psp_input.cpp $(PSP)/psp_time.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/watch_hud.cpp $(PSP)/hive_net.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp $(PSP_XPORT) \
		-o /tmp/t28 && /tmp/t28
	$(CXX) $(CXXFLAGS) $(PEER_INC) $(T)/test_v2_final.cpp \
		$(PSP)/ghost_terminal.cpp $(PSP)/psp_input.cpp $(PSP)/psp_time.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
		$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
		$(PSP)/watch_hud.cpp $(PSP)/hive_net.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp src/laptop/peer_keys.cpp $(PSP_XPORT) \
		-o /tmp/t29 && /tmp/t29

host:
	$(CXX) $(CXXFLAGS) $(PSP)/main.cpp $(PSP_CORE) $(PSP_XPORT) $(PEERS) -o /tmp/ghost_hive_kernel

# Host-only: same kernel as `host`, Watch Hive/Kernel/Net/Peer stacked (no IBSS).
mini:
	$(CXX) $(CXXFLAGS) -DGHOST_MINI_WATCH -I src/laptop $(PSP)/main.cpp $(PSP_CORE) $(PSP_XPORT) $(PEERS) \
		src/laptop/host_telem.cpp -o /tmp/ghost_mini

pack-live:
	python3 scripts/pack_live.py

peers:
	$(CXX) $(CXXFLAGS) $(PEER_INC) src/laptop/main.cpp $(LAPTOP_MODS) src/mine/mine.cpp \
		$(PSP)/ghost_keys.cpp $(PSP)/ghost_crypto.cpp $(PSP)/ghost_telemetry.cpp \
		$(PEER_XPORT) -o /tmp/ghost_laptop
	$(CXX) $(CXXFLAGS) $(PEER_INC) src/phone/main.cpp src/phone/sensor.cpp \
		src/laptop/alert.cpp src/laptop/peer_keys.cpp src/laptop/peer_halt.cpp src/laptop/tetact.cpp src/laptop/host_telem.cpp src/mine/mine.cpp \
		$(PSP)/ghost_keys.cpp $(PSP)/ghost_crypto.cpp $(PSP)/ghost_telemetry.cpp \
		$(PEER_XPORT) -o /tmp/ghost_phone
	$(CXX) $(CXXFLAGS) $(PEER_INC) src/sensor/main.cpp src/phone/sensor.cpp \
		src/laptop/peer_keys.cpp src/laptop/peer_halt.cpp src/laptop/tetact.cpp src/laptop/host_telem.cpp \
		$(PSP)/ghost_keys.cpp $(PSP)/ghost_crypto.cpp $(PSP)/ghost_telemetry.cpp \
		$(PEER_XPORT) -o /tmp/ghost_family
	$(CXX) $(CXXFLAGS) $(PEER_INC) src/router/main.cpp src/router/net_sensor.cpp \
		src/phone/sensor.cpp src/laptop/peer_keys.cpp src/laptop/peer_halt.cpp src/laptop/tetact.cpp src/laptop/host_telem.cpp src/mine/router_mine.cpp src/mine/mine.cpp \
		$(PSP)/ghost_keys.cpp $(PSP)/ghost_crypto.cpp $(PSP)/ghost_telemetry.cpp \
		$(PEER_XPORT) -o /tmp/ghost_router
	$(CXX) $(CXXFLAGS) $(PEER_INC) src/nas/main.cpp src/nas/safe.cpp src/nas/index.cpp \
		src/nas/access.cpp src/nas/honeypot.cpp src/mine/mine.cpp src/laptop/peer_keys.cpp src/laptop/peer_halt.cpp \
		$(PSP)/ghost_keys.cpp $(PSP)/ghost_crypto.cpp \
		$(PEER_XPORT) -o /tmp/ghost_nas
	$(CXX) $(CXXFLAGS) $(PEER_INC) src/mine/main.cpp src/mine/mine.cpp \
		src/mine/browser_mine.cpp src/mine/os_mine.cpp src/mine/iot_mine.cpp \
		src/laptop/peer_keys.cpp \
		$(PSP)/ghost_keys.cpp $(PSP)/ghost_crypto.cpp \
		$(PEER_XPORT) -o /tmp/ghost_mines
	$(CXX) $(CXXFLAGS) $(PEER_INC) src/laptop/keyd.cpp src/laptop/peer_keys.cpp \
		$(PSP)/ghost_keys.cpp $(PSP)/ghost_crypto.cpp -o /tmp/ghost_keyd
	$(CXX) $(CXXFLAGS) $(PEER_INC) src/laptop/ghost_mon.cpp src/laptop/alert.cpp \
		src/laptop/log_client.cpp -o /tmp/ghost_mon
	$(CXX) $(CXXFLAGS) $(PEER_INC) src/laptop/ghost_relay.cpp \
		$(PSP)/transport/transport_frame.cpp -o /tmp/ghost_relay

eboot:
	PSPDEV="$(HOME)/pspdev" PATH="$(HOME)/pspdev/bin:$(PATH)" $(MAKE) -C src/psp

# Scale: 1 Worker / 1 NAS / 1 Router / 1 Phone / ≤8 Sensoren / 1–N Minen (Registry ≤ 32).
# Mehr Honeypots = mehr Minen-IDs, keine zweiten Worker/NAS/Router.
#   /tmp/ghost_mines <kernel-ip> browser id=MB1
#   /tmp/ghost_mines <kernel-ip> os id=MO1
#   /tmp/ghost_laptop <kernel-ip> W mine=ML1 mine=ML2
#   /tmp/ghost_phone <kernel-ip> P mine=MP1 mine=MP2
#   /tmp/ghost_router <kernel-ip> R decoy mine=MR1 mine=MR2
#   /tmp/ghost_nas <kernel-ip> N lockvogel mine=MN1 mine=MN2
#   /tmp/ghost_family <kernel-ip> F
#   /tmp/ghost_family <kernel-ip> F2

# --- Ghost Attack Lab (simulator only, 127.0.0.1) ---
LAB_INC = -I src/psp -I src/laptop -I src/mine -I invariants -I engines
LAB_SAN = -fsanitize=address,undefined -fno-omit-frame-pointer -g
LAB_BIN = /tmp/ghost_lab/bin
LAB_CORE = \
	$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp $(PSP)/ghost_policy.cpp \
	$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp $(PSP)/replay_guard.cpp \
	$(PSP)/decision_pipeline.cpp $(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
	$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp $(PSP)/ghost_output.cpp \
	$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
	$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp \
	src/laptop/peer_keys.cpp \
	invariants/invariants.cpp engines/lab_common.cpp \
	$(PSP_XPORT)

lab-invariants:
	mkdir -p $(LAB_BIN)
	$(CXX) $(CXXFLAGS) -I invariants invariants/test_invariants.cpp invariants/invariants.cpp \
		-o $(LAB_BIN)/test_invariants && $(LAB_BIN)/test_invariants
	$(CXX) $(CXXFLAGS) -I invariants invariants/check_cli.cpp invariants/invariants.cpp \
		-o $(LAB_BIN)/inv_check

lab-sim: lab-invariants
	mkdir -p $(LAB_BIN) /tmp/ghost_lab
	$(CXX) $(CXXFLAGS) $(LAB_SAN) $(LAB_INC) sim/ghost_sim.cpp $(LAB_CORE) \
		-o $(LAB_BIN)/ghost-sim

lab-engines: lab-invariants
	mkdir -p $(LAB_BIN) engines/a_fuzzer/corpus
	$(CXX) $(CXXFLAGS) $(LAB_INC) engines/a_fuzzer/gen_corpus.cpp $(LAB_CORE) \
		-o $(LAB_BIN)/gen_corpus && $(LAB_BIN)/gen_corpus
	$(CXX) $(CXXFLAGS) $(LAB_INC) engines/a_fuzzer/fuzz_wire.cpp $(LAB_CORE) \
		-o $(LAB_BIN)/fuzz_smoke && $(LAB_BIN)/fuzz_smoke engines/a_fuzzer/corpus
	$(CXX) $(CXXFLAGS) $(LAB_INC) engines/b_guided/engine.cpp $(LAB_CORE) -o $(LAB_BIN)/engine_b
	$(CXX) $(CXXFLAGS) $(LAB_INC) engines/c_replay/engine.cpp $(LAB_CORE) -o $(LAB_BIN)/engine_c
	$(CXX) $(CXXFLAGS) $(LAB_INC) engines/d_desync/engine.cpp $(LAB_CORE) -o $(LAB_BIN)/engine_d
	$(CXX) $(CXXFLAGS) $(LAB_INC) engines/e_counter_mac/engine.cpp $(LAB_CORE) -o $(LAB_BIN)/engine_e

lab-fuzz: lab-engines
	@if command -v clang++ >/dev/null 2>&1; then \
		clang++ -std=c++11 -DGHOST_LIBFUZZER -fsanitize=fuzzer,address,undefined \
			$(LAB_INC) engines/a_fuzzer/fuzz_wire.cpp $(LAB_CORE) \
			-o $(LAB_BIN)/fuzz_wire && \
		$(LAB_BIN)/fuzz_wire engines/a_fuzzer/corpus -max_total_time=8 -timeout=2 \
			-artifact_prefix=/tmp/ghost_lab/fuzz_ ; \
	else \
		echo "clang++ missing: skip libFuzzer (fuzz_smoke already ran)"; \
	fi

lab: lab-sim lab-engines

lab-smoke: lab
	python3 controller/controller.py --seconds 12

lab-test-phase: lab
	python3 tests/test_lab_phase.py

lab-24h: lab
	sh controller/run_24h.sh

# Phase A: 24 real hours, sim_time_factor=200, ghost_mode inside simulator only.
lab-24h-accelerated: lab
	@test -x $(LAB_BIN)/fuzz_wire || $(MAKE) lab-fuzz || true
	sh controller/run_24h_accelerated.sh

redteam: export GHOST_DOWN_ARMED=1
redteam:
	python3 $(T)/hive_redteam/gen_attacks.py
	$(CXX) $(CXXFLAGS) -I $(T) $(PEER_INC) $(T)/hive_redteam/run_redteam.cpp \
		$(PSP)/registry.cpp $(PSP)/ghost_telemetry.cpp $(PSP)/event_queue.cpp \
		$(PSP)/ghost_policy.cpp $(PSP)/context_engine.cpp \
		$(PSP)/priority_engine.cpp $(PSP)/fallback_engine.cpp \
		$(PSP)/replay_guard.cpp $(PSP)/decision_pipeline.cpp \
		$(PSP)/ghost_heartbeat.cpp $(PSP)/ghost_scanner.cpp \
		$(PSP)/ghost_stealth.cpp $(PSP)/ghost_peek.cpp \
		$(PSP)/ghost_output.cpp $(PSP)/watch_hud.cpp \
		$(PSP)/ghost_crypto.cpp $(PSP)/ghost_vault.cpp $(PSP)/ghost_keys.cpp \
		$(PSP)/ghost_wrap.cpp $(PSP)/root_config.cpp \
		$(PSP)/ghost_down.cpp $(PSP)/ghost_ir.cpp $(PSP)/psp_time.cpp \
		$(PSP)/hive_net.cpp \
		src/mine/mine.cpp src/mine/os_mine.cpp src/mine/browser_mine.cpp \
		src/laptop/kill.cpp src/laptop/alert.cpp src/laptop/peer_keys.cpp \
		$(PSP_XPORT) \
		-o /tmp/hive_redteam && /tmp/hive_redteam

