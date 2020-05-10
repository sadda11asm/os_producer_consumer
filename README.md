# os_producer_consumer
## Producer-Consumer multiplexing server written in C using semaphores, mutex, threads, sockets, POSIX etc.

### Two main clients: 
```
consumers.c, producers.c
```
#### Main multiplexing server:
```
procon_server.c
```

## To compile needed files with Makefile:
```bash
make all
```
### To run server:
```bash
./pcserver [host] port buffer_size
```
### To run producers:
```bash
./producers [host] port number_of_clients rate percentage_of_bad_clients
```
### To run consumers:
```bash
./consumers [host] port number_of_clients rate percentage_of_bad_clients
```


