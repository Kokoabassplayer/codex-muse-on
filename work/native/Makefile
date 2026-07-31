CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic
PLATFORM_FRAMEWORKS = -framework AppKit -framework ApplicationServices -framework Carbon
HID_FRAMEWORKS = -framework IOKit -framework CoreFoundation
PHASE2_SOURCES = muse_on_decoder.c muse_on_action_map.c muse_on_config.c \
	muse_on_activation.c muse_on_shortcut_map.c muse_on_key_filter.c \
	muse_on_capture.c muse_on_listener_lifecycle.c \
	muse_on_platform.m muse_on_state_coordinator.c muse_on_connection.c
TEST_BINARIES = muse_on_decoder_test muse_on_action_map_test \
	muse_on_config_test muse_on_activation_test muse_on_shortcut_map_test \
	muse_on_platform_test muse_on_key_filter_test muse_on_state_coordinator_test \
	muse_on_setup_state_test muse_on_connection_test muse_on_diagnostics_test \
	muse_on_controller_map_test muse_on_connection_observer_test \
	muse_on_quit_policy_test muse_on_permission_test \
	muse_on_listener_completion_gate_test muse_on_topology_test \
	muse_on_capture_test muse_on_focus_lifecycle_test muse_on_protocol_test
FOCUSED_TEST_BINARIES = muse_on_state_coordinator_test muse_on_connection_test \
	muse_on_key_filter_test muse_on_platform_test muse_on_controller_map_test \
	muse_on_connection_observer_test muse_on_quit_policy_test \
	muse_on_permission_test muse_on_listener_completion_gate_test \
	muse_on_topology_test muse_on_capture_test muse_on_focus_lifecycle_test \
	muse_on_protocol_test
ANALYZER_REPORTS = muse_on_listener.plist muse_on_platform.plist \
	muse_on_key_filter.plist muse_on_capture.plist muse_on_state_coordinator.plist \
	muse_on_listener_lifecycle.plist \
	muse_on_setup_state.plist muse_on_connection.plist \
	muse_on_topology.plist \
	muse_on_diagnostics.plist muse_on_menu_bar.plist \
	muse_on_connection_observer.plist
SANITIZER_FLAGS ?= -fsanitize=address,undefined

test: verify-listener-lifecycle-binding $(TEST_BINARIES)
	./muse_on_decoder_test
	./muse_on_action_map_test
	./muse_on_config_test
	./muse_on_activation_test
	./muse_on_shortcut_map_test
	./muse_on_platform_test
	./muse_on_permission_test
	./muse_on_key_filter_test
	./muse_on_state_coordinator_test
	./muse_on_setup_state_test
	./muse_on_connection_test
	./muse_on_diagnostics_test
	./muse_on_controller_map_test
	./muse_on_connection_observer_test
	./muse_on_quit_policy_test
	./muse_on_listener_completion_gate_test
	./muse_on_topology_test
	./muse_on_capture_test
	./muse_on_focus_lifecycle_test
	./muse_on_protocol_test

test-focused: verify-listener-lifecycle-binding $(FOCUSED_TEST_BINARIES)
	./muse_on_state_coordinator_test
	./muse_on_connection_test
	./muse_on_key_filter_test
	./muse_on_platform_test
	./muse_on_permission_test
	./muse_on_controller_map_test
	./muse_on_connection_observer_test
	./muse_on_quit_policy_test
	./muse_on_listener_completion_gate_test
	./muse_on_topology_test
	./muse_on_capture_test
	./muse_on_focus_lifecycle_test
	./muse_on_protocol_test

test-sanitize:
	@set -eu; \
	$(MAKE) clean; \
	$(MAKE) CFLAGS="$(CFLAGS) $(SANITIZER_FLAGS)" test; \
	$(MAKE) clean

muse_on_decoder_test: muse_on_decoder.c tests/muse_on_decoder_test.c muse_on_decoder.h
	$(CC) $(CFLAGS) -I. muse_on_decoder.c tests/muse_on_decoder_test.c -o $@

muse_on_action_map_test: muse_on_action_map.c muse_on_decoder.c \
		tests/muse_on_action_map_test.c muse_on_action_map.h muse_on_decoder.h
	$(CC) $(CFLAGS) -I. muse_on_action_map.c muse_on_decoder.c \
		tests/muse_on_action_map_test.c -o $@

muse_on_config_test: muse_on_config.c tests/muse_on_config_test.c muse_on_config.h muse_on_action_map.h
	$(CC) $(CFLAGS) -I. muse_on_config.c tests/muse_on_config_test.c -o $@

muse_on_activation_test: muse_on_activation.c tests/muse_on_activation_test.c muse_on_activation.h muse_on_config.h
	$(CC) $(CFLAGS) -I. muse_on_activation.c tests/muse_on_activation_test.c -o $@

muse_on_shortcut_map_test: muse_on_shortcut_map.c muse_on_action_map.c \
		tests/muse_on_shortcut_map_test.c muse_on_shortcut_map.h muse_on_action_map.h
	$(CC) $(CFLAGS) -I. muse_on_shortcut_map.c muse_on_action_map.c \
		tests/muse_on_shortcut_map_test.c -o $@

muse_on_platform_test: muse_on_platform.m tests/muse_on_platform_test.m muse_on_platform.h muse_on_shortcut_map.h
	$(CC) $(CFLAGS) -I. muse_on_platform.m tests/muse_on_platform_test.m \
		$(PLATFORM_FRAMEWORKS) $(HID_FRAMEWORKS) -o $@

muse_on_permission_test: muse_on_platform.m muse_on_state_coordinator.c tests/muse_on_permission_test.c muse_on_platform.h muse_on_shortcut_map.h muse_on_state_coordinator.h
	$(CC) $(CFLAGS) -I. muse_on_platform.m muse_on_state_coordinator.c \
		tests/muse_on_permission_test.c \
		$(PLATFORM_FRAMEWORKS) $(HID_FRAMEWORKS) -o $@

muse_on_key_filter_test: muse_on_key_filter.c tests/muse_on_key_filter_test.c muse_on_key_filter.h
	$(CC) $(CFLAGS) -I. muse_on_key_filter.c tests/muse_on_key_filter_test.c \
		$(HID_FRAMEWORKS) -o $@

muse_on_state_coordinator_test: muse_on_state_coordinator.c \
	tests/muse_on_state_coordinator_test.c muse_on_state_coordinator.h
	$(CC) $(CFLAGS) -I. muse_on_state_coordinator.c \
		tests/muse_on_state_coordinator_test.c -o $@

muse_on_setup_state_test: muse_on_setup_state.c tests/muse_on_setup_state_test.c \
		muse_on_setup_state.h
	$(CC) $(CFLAGS) -I. muse_on_setup_state.c \
		tests/muse_on_setup_state_test.c -o $@

muse_on_connection_test: muse_on_connection.c tests/muse_on_connection_test.c \
		muse_on_connection.h
	$(CC) $(CFLAGS) -I. muse_on_connection.c \
		tests/muse_on_connection_test.c -o $@

muse_on_connection_observer_test: muse_on_connection.c muse_on_connection_observer.m \
		tests/muse_on_connection_observer_test.m muse_on_connection.h \
		muse_on_connection_observer.h
	$(CC) $(CFLAGS) -DMUSE_ON_CONNECTION_OBSERVER_TESTING -fobjc-arc -I. muse_on_connection.c \
		muse_on_connection_observer.m tests/muse_on_connection_observer_test.m \
		-framework Foundation -framework IOKit -framework CoreFoundation -o $@

muse_on_diagnostics_test: muse_on_diagnostics.c tests/muse_on_diagnostics_test.c \
		muse_on_diagnostics.h muse_on_state_coordinator.h muse_on_action_map.h
	$(CC) $(CFLAGS) -I. muse_on_diagnostics.c muse_on_state_coordinator.c \
		muse_on_action_map.c tests/muse_on_diagnostics_test.c -o $@

muse_on_controller_map_test: muse_on_action_map.c \
		tests/muse_on_controller_map_test.c muse_on_action_map.h muse_on_decoder.h
	$(CC) $(CFLAGS) -I. muse_on_action_map.c \
		tests/muse_on_controller_map_test.c -o $@

muse_on_quit_policy_test: muse_on_quit_policy.c \
		tests/muse_on_quit_policy_test.c muse_on_quit_policy.h
	$(CC) $(CFLAGS) -I. muse_on_quit_policy.c \
		tests/muse_on_quit_policy_test.c -o $@

muse_on_listener_completion_gate_test: tests/muse_on_listener_completion_gate_test.c \
	muse_on_listener_completion_gate.h
	$(CC) $(CFLAGS) -I. tests/muse_on_listener_completion_gate_test.c -o $@

muse_on_topology_test: muse_on_topology.c muse_on_connection.c \
	tests/muse_on_topology_test.c muse_on_topology.h muse_on_connection.h \
	muse_on_state_coordinator.c muse_on_state_coordinator.h
	$(CC) $(CFLAGS) -I. muse_on_topology.c muse_on_connection.c \
		muse_on_state_coordinator.c tests/muse_on_topology_test.c -o $@

muse_on_capture_test: muse_on_capture.c muse_on_action_map.c muse_on_decoder.c \
		tests/muse_on_capture_test.c muse_on_capture.h
	$(CC) $(CFLAGS) -I. muse_on_capture.c muse_on_action_map.c \
		muse_on_decoder.c \
		tests/muse_on_capture_test.c $(HID_FRAMEWORKS) -o $@

muse_on_focus_lifecycle_test: muse_on_listener_lifecycle.c muse_on_capture.c muse_on_action_map.c \
		muse_on_decoder.c muse_on_topology.c muse_on_connection.c \
		muse_on_state_coordinator.c tests/muse_on_focus_lifecycle_test.c \
		muse_on_listener_lifecycle.h muse_on_capture.h muse_on_action_map.h muse_on_decoder.h \
		muse_on_topology.h muse_on_connection.h muse_on_state_coordinator.h
	$(CC) $(CFLAGS) -I. muse_on_listener_lifecycle.c muse_on_capture.c muse_on_action_map.c \
		muse_on_decoder.c muse_on_topology.c muse_on_connection.c \
		muse_on_state_coordinator.c tests/muse_on_focus_lifecycle_test.c \
		$(HID_FRAMEWORKS) -o $@

muse_on_protocol_test: tests/muse_on_protocol_test.m muse_on_protocol.h
	$(CC) $(CFLAGS) -fobjc-arc -I. tests/muse_on_protocol_test.m \
		-framework Foundation -o $@

muse_on_listener_binding.o: muse_on_listener.c muse_on_listener_lifecycle.h
	$(CC) $(CFLAGS) -I. -c muse_on_listener.c -o $@

verify-listener-lifecycle-binding: muse_on_listener_binding.o
	@set -eu; \
	symbols="$$(nm -u muse_on_listener_binding.o)"; \
	printf '%s\n' "$$symbols" | grep -Eq '(_)?muse_on_listener_lifecycle_focus_changed$$'; \
	printf '%s\n' "$$symbols" | grep -Eq '(_)?muse_on_listener_lifecycle_observe_report$$'; \
	printf '%s\n' "$$symbols" | grep -Eq '(_)?muse_on_listener_lifecycle_tick$$'; \
	printf '%s\n' 'Listener lifecycle binding: PASS'

listener: muse_on_listener.c $(PHASE2_SOURCES) muse_on_topology.c
	$(CC) $(CFLAGS) -I. muse_on_listener.c $(PHASE2_SOURCES) muse_on_topology.c \
		$(HID_FRAMEWORKS) $(PLATFORM_FRAMEWORKS) -o muse_on_listener

analyze:
	$(CC) --analyze $(CFLAGS) -I. muse_on_listener.c
	$(CC) --analyze $(CFLAGS) -I. muse_on_platform.m
	$(CC) --analyze $(CFLAGS) -I. muse_on_key_filter.c
	$(CC) --analyze $(CFLAGS) -I. muse_on_capture.c
	$(CC) --analyze $(CFLAGS) -I. muse_on_listener_lifecycle.c
	$(CC) --analyze $(CFLAGS) -I. muse_on_state_coordinator.c
	$(CC) --analyze $(CFLAGS) -I. muse_on_setup_state.c
	$(CC) --analyze $(CFLAGS) -I. muse_on_connection.c
	$(CC) --analyze $(CFLAGS) -I. muse_on_topology.c
	$(CC) --analyze $(CFLAGS) -fobjc-arc -I. muse_on_connection_observer.m
	$(CC) --analyze $(CFLAGS) -I. muse_on_diagnostics.c
	$(CC) --analyze $(CFLAGS) -fobjc-arc -I. muse_on_menu_bar.m

package:
	./package_app.sh

verify-bundle: package
	./tests/verify_bundle.sh

verify-v1:
	@set -eu; \
	cleanup() { $(MAKE) clean; }; \
	trap cleanup EXIT INT TERM; \
	$(MAKE) clean; \
	$(MAKE) test-focused; \
	$(MAKE) test; \
	$(MAKE) test-sanitize; \
	$(MAKE) analyze; \
	$(MAKE) listener; \
	$(MAKE) verify-bundle; \
	$(MAKE) clean; \
	trap - EXIT INT TERM; \
	printf '%s\n' 'Codex Muse-On v1 verification: PASS'

clean:
	rm -f $(TEST_BINARIES) $(TEST_BINARIES:%=%_san) \
		muse_on_listener muse_on_listener_binding.o $(ANALYZER_REPORTS)

.PHONY: test test-focused test-sanitize clean listener analyze package \
	verify-bundle verify-listener-lifecycle-binding verify-v1
