# Extra Credit Part for Project 3
## Intructions to Compile and Run
```bash
cd cache #Working directory
make
./webproxy
#Then, in a different shell
./simple_cached
#And still in a different shell
./gfclient_download
```

## Notes
Multithreading support is now implemented in `gfserver` library. It has almost the same API with `gfserver.h` provided in project 3. Consequently the code from PR3 does not need to be changed at all.

No change is made to `gfclient` library. Multithreading of `gfclient_download` is implemented in `gfclient_download.c`
