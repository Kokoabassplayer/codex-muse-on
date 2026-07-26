CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic
PLATFORM_FRAMEWORKS = -framework AppKit -framework ApplicationServices -framework Carbon
HID_FRAMEWORKS = -framework IOKit -framework CoreFoundation
PHASE2_SOURCES = muse_on_decoder.c muse_on_action_map.c muse_on_config.c \
	muse_on_activation.c muse_on_shortcut_map.c muse_on_key_filter.c \
	muse_on_platform.m muse_on_state_coordinator.c muse_on_connection.c
TEST_BINARIES = muse_on_decoder_test muse_on_action_map_test \
	muse_on_config_test muse_on_activation_test muse_on_shortcut_map_test \
	muse_on_platform_test muse_on_key_filter_test muse_on_state_coordinator_test \
	muse_on_setup_state_test muse_on_connection_test muse_on_diagnostics_test \
	muse_on_controller_map_test
FOCUSED_TEST_BINARIES = muse_on_state_coordinator_test muse_on_connection_test \
	muse_on_key_filter_test muse_on_platform_test muse_on_controller_map_test
ANALYZER_REPORTS = muse_on_listener.plist muse_on_platform.plist \
	muse_on_key_filter.plist muse_on_state_coordinator.plist \
	muse_on_setup_state.plist muse_on_connection.plist \
	muse_on_diagnostics.plist muse_on_menu_bar.plist
SANITIZER_FLAGS ?= -fsanitize=address,undefined

test: $(TEST_BINARIES)
	./muse_on_decoder_test
	./muse_on_action_map_test
	./muse_on_config_test
	./muse_on_activation_test
	./muse_on_shortcut_map_test
	./muse_on_platform_test
	./muse_on_key_filter_test
	./muse_on_state_coordinator_test
	./muse_on_setup_state_test
	./muse_on_connection_test
	./muse_on_diagnostics_test
	./muse_on_controller_map_test

test-focused: $(FOCUSED_TEST_BINARIES)
	./muse_on_state_coordinator_test
	./muse_on_connection_test
	./muse_on_key_filter_test
	./muse_on_platform_test
	./muse_on_controller_map_test

test-sanitize:
	@set -eu; \
	$(MAKE) clean; \
	$(MAKE) CFLAGS="$(CFLAGS) $(SANITIZER_FLAGS)" test; \
	$(MAKE) clean

muse_on_decoder_test: muse_on_decoder.c tests/muse_on_decoder_test.c muse_on_decoder.h
	$(CC) $(CFLAGS) -I. muse_on_decoder.c tests/muse_on_decoder_test.c -o $@

muse_on_action_map_test: muse_on_action_map.c tests/muse_on_action_map_test.c muse_on_action_map.h muse_on_decoder.h
	$(CC) $(CFLAGS) -I. muse_on_action_map.c tests/muse_on_action_map_test.c -o $@

muse_on_config_test: muse_on_config.c tests/muse_on_config_test.c muse_on_config.h muse_on_action_map.h
	$(CC) $(CFLAGS) -I. muse_on_config.c tests/muse_on_config_test.c -o $@

muse_on_activation_test: muse_on_activation.c tests/muse_on_activation_test.c muse_on_activation.h muse_on_config.h
	$(CC) $(CFLAGS) -I. muse_on_activation.c tests/muse_on_activation_test.c -o $@

muse_on_shortcut_map_test: muse_on_shortcut_map.c tests/muse_on_shortcut_map_test.c muse_on_shortcut_map.h muse_on_action_map.h
	$(CC) $(CFLAGS) -I. muse_on_shortcut_map.c tests/muse_on_shortcut_map_test.c -o $@

muse_on_platform_test: muse_on_platform.m tests/muse_on_platform_test.m muse_on_platform.h muse_on_shortcut_map.h
	$(CC) $(CFLAGS) -I. muse_on_platform.m tests/muse_on_platform_test.m \
		$(PLATFORM_FRAMEWORKS) -o $@

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

muse_on_diagnostics_test: muse_on_diagnostics.c tests/muse_on_diagnostics_test.c \
		muse_on_diagnostics.h muse_on_state_coordinator.h muse_on_action_map.h
	$(CC) $(CFLAGS) -I. muse_on_diagnostics.c muse_on_state_coordinator.c \
		muse_on_action_map.c tests/muse_on_diagnostics_test.c -o $@

muse_on_controller_map_test: muse_on_action_map.c \
		tests/muse_on_controller_map_test.c muse_on_action_map.h muse_on_decoder.h
	$(CC) $(CFLAGS) -I. muse_on_action_map.c \
		tests/muse_on_controller_map_test.c -o $@

listener: muse_on_listener.c $(PHASE2_SOURCES)
	$(CC) $(CFLAGS) -I. muse_on_listener.c $(PHASE2_SOURCES) \
		$(HID_FRAMEWORKS) $(PLATFORM_FRAMEWORKS) -o muse_on_listener

analyze:
	$(CC) --analyze $(CFLAGS) -I. muse_on_listener.c
	$(CC) --analyze $(CFLAGS) -I. muse_on_platform.m
	$(CC) --analyze $(CFLAGS) -I. muse_on_key_filter.c
	$(CC) --analyze $(CFLAGS) -I. muse_on_state_coordinator.c
	$(CC) --analyze $(CFLAGS) -I. muse_on_setup_state.c
	$(CC) --analyze $(CFLAGS) -I. muse_on_connection.c
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
		muse_on_listener $(ANALYZER_REPORTS)

.PHONY: test test-focused test-sanitize clean listener analyze package verify-bundle verify-v1
