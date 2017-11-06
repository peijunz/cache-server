CC     = gcc
ASAN_FLAGS = -fsanitize=address -fno-omit-frame-pointer -Wno-format-security
ASAN_LIBS = -static-libasan
CFLAGS := -Wall -Werror --std=gnu99 -g3

ARCH := $(shell uname)
ifneq ($(ARCH),Darwin)
  LDFLAGS += -lpthread
endif

# default is to build with address sanitizer enabled
all: gfclient_download

# the noasan version can be used with valgrind
all_noasan: gfclient_download_noasan

gfclient_download: gfclient.o workload.o gfclient_download.o steque.o
	$(CC) -o $@ $(CFLAGS) $(ASAN_FLAGS) $^ $(LDFLAGS)  $(ASAN_LIBS)

gfclient_download_noasan: gfclient.o workload_noasan.o gfclient_download_noasan.o steque_noasan.o
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

%_noasan.o : %.c
	$(CC) -c -o $@ $(CFLAGS) $<

%.o : %.c
	$(CC) -c -o $@ $(CFLAGS) $(ASAN_FLAGS) $<

.PHONY: clean

clean:
	rm -fr *.o gfclient_download gfclient_download_noasan
