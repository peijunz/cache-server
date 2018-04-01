CFLAGS := -Wall --std=gnu99 -g3 -Werror -fPIC -I../gflib
ASAN_FLAGS = -fsanitize=address -fno-omit-frame-pointer
ASAN_LIBS = -static-libasan
CURL_LIBS := $(shell curl-config --libs)
CURL_CFLAGS := $(shell curl-config --cflags)

ARCH := $(shell uname)
ifneq ($(ARCH),Darwin)
  LDFLAGS += -lpthread -lrt -static-libasan
endif

PROXY_OBJ := webproxy.o gflib/steque.o gflib/gfserver.o
PROXY_OBJ_NOASAN := webproxy_noasan.o gflib/steque_noasan.o gflib/gfserver_noasan.o

all: clean all_asan all_noasan gfclient_download

all_asan: webproxy simplecached

all_noasan: clean webproxy_noasan simplecached_noasan

noasan: all_noasan

webproxy: $(PROXY_OBJ) handle_with_cache.o shm_channel.o gflib/gfserver.o 
	$(CC) -o $@ $(CFLAGS) $(ASAN_FLAGS) $(CURL_CFLAGS) $^ $(LDFLAGS) $(CURL_LIBS) $(ASAN_LIBS)

simplecached: simplecache.o simplecached.o shm_channel.o gflib/steque.o
	$(CC) -o $@ $(CFLAGS) $(ASAN_FLAGS) $^ $(LDFLAGS) $(ASAN_LIBS)

webproxy_noasan: $(PROXY_OBJ_NOASAN) handle_with_cache_noasan.o shm_channel_noasan.o gflib/gfserver_noasan.o 
	$(CC) -o $@ $(CFLAGS) $(CURL_CFLAGS) $^ $(LDFLAGS) $(CURL_LIBS)

simplecached_noasan: simplecache_noasan.o simplecached_noasan.o shm_channel_noasan.o gflib/steque_noasan.o
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

%_noasan.o : %.c
	$(CC) -c -o $@ $(CFLAGS) $<

%.o : %.c
	$(CC) -c -o $@ $(CFLAGS) $(ASAN_FLAGS) $<


gflib/%_noasan.o : gflib/%.c
	$(CC) -c -o $@ $(CFLAGS) $<

gflib/%.o : gflib/%.c
	$(CC) -c -o $@ $(CFLAGS) $(ASAN_FLAGS) $<

gfclient_download:
	cd ../gflib;make gfclient_download;
	ln -s ../gflib/gfclient_download gfclient_download
.PHONY: clean

clean:
# 	mv gfserver.o gfserver.tmpo 
# 	mv gfserver_noasan.o gfserver_noasan.tmpo
	rm -rf *.o webproxy simplecached webproxy_noasan simplecached_noasan .shm-file-* gfclient_download
# 	mv gfserver.tmpo gfserver.o
# 	mv gfserver_noasan.tmpo gfserver_noasan.o
