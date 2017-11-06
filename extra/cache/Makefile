CFLAGS := -Wall --std=gnu99 -g3 -Werror -fPIC
ASAN_FLAGS = -fsanitize=address -fno-omit-frame-pointer
ASAN_LIBS = -static-libasan
CURL_LIBS := $(shell curl-config --libs)
CURL_CFLAGS := $(shell curl-config --cflags)

ARCH := $(shell uname)
ifneq ($(ARCH),Darwin)
  LDFLAGS += -lpthread -lrt -static-libasan
endif

PROXY_OBJ := webproxy.o steque.o
PROXY_OBJ_NOASAN := webproxy_noasan.o steque_noasan.o

all: clean all_asan all_noasan

all_asan: webproxy simplecached gfclient_download

all_noasan: clean webproxy_noasan simplecached_noasan gfclient_download_noasan

noasan: all_noasan

webproxy: $(PROXY_OBJ) handle_with_cache.o shm_channel.o gfserver.o gf-student.o
	$(CC) -o $@ $(CFLAGS) $(ASAN_FLAGS) $(CURL_CFLAGS) $^ $(LDFLAGS) $(CURL_LIBS) $(ASAN_LIBS)

simplecached: simplecache.o simplecached.o shm_channel.o steque.o
	$(CC) -o $@ $(CFLAGS) $(ASAN_FLAGS) $^ $(LDFLAGS) $(ASAN_LIBS)

gfclient_download: workload.o gfclient_download.o steque.o gfclient.o gf-student.o
	$(CC) -o $@ $(CFLAGS) $(ASAN_FLAGS) $^ $(LDFLAGS)  $(ASAN_LIBS)

webproxy_noasan: $(PROXY_OBJ_NOASAN) handle_with_cache_noasan.o shm_channel_noasan.o gfserver_noasan.o gf-student_noasan.o
	$(CC) -o $@ $(CFLAGS) $(CURL_CFLAGS) $^ $(LDFLAGS) $(CURL_LIBS)

simplecached_noasan: simplecache_noasan.o simplecached_noasan.o shm_channel_noasan.o steque_noasan.o
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

gfclient_download_noasan: workload_noasan.o gfclient_download_noasan.o steque_noasan.o gfclient_noasan.o gf-student_noasan.o
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

%.o : %.c
	$(CC) -c -o $@ $(CFLAGS) $(ASAN_FLAGS) $<

%_noasan.o : %.c
	$(CC) -c -o $@ $(CFLAGS) $<

.PHONY: clean

clean:
	rm -rf *.o webproxy simplecached gfclient_download webproxy_noasan simplecached_noasan gfclient_download_noasan .shm-file-*
