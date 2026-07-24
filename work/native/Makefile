CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic
PLATFORM_FRAMEWORKS = -framework AppKit -framework ApplicationServices -framework Carbon
HID_FRAMEWORKS = -framework IOKit -framework CoreFoundation
PHASE2_SOURCES = muse_on_decoder.c muse_on_action_map.c muse_on_config.c \
	muse_on_activation.c muse_on_shortcut_map.c muse_on_key_filter.c \
	muse_on_platform.m
TEST_BINARIES = muse_on_decoder_test muse_on_action_map_test \
	muse_on_config_test muse_on_activation_test muse_on_shortcut_map_test \
	muse_on_platform_test muse_on_key_filter_test

test: $(TEST_BINARIES)
	./muse_on_decoder_test
	./muse_on_action_map_test
	./muse_on_config_test
	./muse_on_activation_test
	./muse_on_shortcut_map_test
	./muse_on_platform_test
	./muse_on_key_filter_test

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

listener: muse_on_listener.c $(PHASE2_SOURCES)
	$(CC) $(CFLAGS) -I. muse_on_listener.c $(PHASE2_SOURCES) \
		$(HID_FRAMEWORKS) $(PLATFORM_FRAMEWORKS) -o muse_on_listener

analyze:
	$(CC) --analyze $(CFLAGS) -I. muse_on_listener.c
	$(CC) --analyze $(CFLAGS) -I. muse_on_platform.m
	$(CC) --analyze $(CFLAGS) -I. muse_on_key_filter.c

clean:
	rm -f $(TEST_BINARIES) $(TEST_BINARIES:%=%_san) \
		muse_on_listener

.PHONY: test clean listener analyze
