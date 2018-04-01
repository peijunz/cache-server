CFLAGS := -Wall --std=gnu99 -g3 -Werror -fPIC -I../gflib
ASAN_FLAGS = -fsanitize=address -fno-omit-frame-pointer
ASAN_LIBS = -static-libasan
CURL_LIBS := $(shell curl-config --libs)
CURL_CFLAGS := $(shell curl-config --cflags)
ARCH := $(shell uname)
ifneq ($(ARCH),Darwin)
  LDFLAGS += -lpthread -lrt
endif

PROXY_OBJ := webproxy.o gflib/steque.o handle_with_curl.o gflib/gfserver.o
PROXY_OBJ_NOASAN := webproxy_noasan.o gflib/steque_noasan.o handle_with_curl_noasan.o gflib/gfserver_noasan.o

all: clean all_asan all_noasan gfclient_download

all_asan: webproxy

all_noasan: webproxy_noasan

noasan: all_noasan

webproxy: $(PROXY_OBJ)
	$(CC) -o $@ $(CFLAGS) $(ASAN_FLAGS) $(CURL_CFLAGS) $^ $(LDFLAGS) $(CURL_LIBS) $(ASAN_LIBS)

webproxy_noasan: $(PROXY_OBJ_NOASAN) 
	$(CC) -o $@ $(CFLAGS) $(CURL_CFLAGS) $^ $(LDFLAGS) $(CURL_LIBS)

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
	rm -rf *.o webproxy webproxy_noasan gfclient_download
	rm -rf courses/
