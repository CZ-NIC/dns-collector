# Dns Collector

A collector for DNS queries than matches observed queries with the responses.

## Features

* Reading capture files and live traces that [libtrace reads](http://www.wand.net.nz/trac/libtrace/wiki/SupportedTraceFormats), including kernel ringbuffer. Configurable packet filter.
* Fast multithreaded processing (3 threads): up to 150 000 queries/s offline and offline (on i5 2.4 GHz, see benchmarks below).
* Pcap dumps of invalid packets with rate-limiting, compression and output file rotation.
* Configurable CSV output compatible with Entrada packet parser, modular output allows easy implementation of other output formats.
* Automatic output file rotation and compression or other post-processing (any pipe command).
* Easy configuration via config files, periodic status and statistics logging, graceful shutdown on first SIGINT.
* Well documented and clean code, licenced under GNU GPL v3.

### Extracted DNS features 

* Timing of request and response (microsecond accuracy)
* Network information (addresses, ports, protocols, TTL, packet and payload sizes)
* DNS header information (query information, id, flags, rcode, opcode, RR counts)
* EDNS information (version, extended flags and rcode, DAU, DHU, N3U, NSID)

### Not yet implemented

* TCP flow recoinstruction for reused streams (currently only single-query TCP connections are supported)
* IP(4/6) fragmented packet reconstuction, DNS via ICMP
* More EDNS features: Client subnet, other options
* Idea: slightly delay the responses in the workflow (cca 10 us, to remove capture timing jitter)

## Requirements

These are Ubuntu package names, but should be similar in other distros.

* Clang or GCC build environmrnt, make
* `libtrace-dev` 3.x (tested with 3.0.21)
* `libknot-dev` 2.3+ (tested with 2.3.0) Use [Knot PPA](https://launchpad.net/~cz.nic-labs/+archive/ubuntu/knot-dns) for Ubuntu (`libknot-dev 2.1.1` in Ubuntu multiverse is broken)
* Optionally tcmalloc (from package `libgoogle-perftools-dev`, tested with ver 2.4) for faster allocation and cca 20% speedup (to use set `USE_TCMALLOC` in `src/Makefile`)
* [LibUCW](http://www.ucw.cz/libucw/) 6.5+ is included as git submodule and fetched and built automatically

## Building and running

* set `USE_TCMALLOC=1` in `src/Makefile` if you want dnscol to use tcmalloc
* `make` to compile the dnscol
* `make docs` to generate developer Doxygen documentation in `docs/html`

* `src/dnscol -C dnscol.conf pcap_files ...` to process offline capture files in sequence.
* `src/dnscol -C dnscol.conf` to process configured live trace, Ctrl+C (SIGINT) to gracefully stop.

## Memory footprint

On 64bit system, the basic memory usage is quite low (cca 30 MB). The only memory-intensive part is the matching window,
which keeps all the packets of the past matching window in memory. Consider this when choosing the matching window.

The total usage is therefore approximately: (30 + 3 * T * Q) MB, where T is the matching window in seconds, and Q the request frequency in kq/s (thousand queries per second).

For example, 10 s matching window and 10 kqps requires 300 MB. In an extremal situation, 10 s matching window and 150 kqps would require 5 GB of memory.

*NOTE:* The current memory usage is 2 kB per request+response packet pair, including parsed packets data, allocation overhead and matching hash table. This could be likely reduced to some 500 B (in the optimistic case of most requests being matched) by early extraction of output-relevant data from the packets before matching (now done on output), but that would complicate the code structure and limit extensibility.

## Benchmarks

*Testing machine:* `knot-master` 2x4 core Intel Xeon, 2.4 GHz. Debian 8.5. Online test packets are replayed from another machine via Intel 10G X520.

### Offline preocessing

Cca 150 000 q/s on authoritative server data (Knot DNS server), 34% DNSSEC requests (very little other EDNS), 71% IPv6, 8% TCP.

Uses 2.5 cores without compression (good compression uses another 1 core). Varies with the selected field set.

### Online processing

Running ont the same node (knot-master) as the Knot DNS server (Knot 2.3). Capturing via kernel ring buffer.

Packet statistics:

| kq per sec | Knot | DnsCol |
| ---- | ---- | ---- |
| 50   | OK | OK |
| 100  | OK | <1% lost |
| 150  | OK | 5%-10% lost |
| ...  | OK | reading 150 kq/s |
| 300  | OK | reading 150 kq/s |
| 500  | <0.2% lost | reading 100-150 kq/s |
| 700  | 15% lost | reading 100-150 kq/s |
| 900  | 50% lost (see below) | reading 100-150 kq/s |

*NOTE:* In this setting, the knot server alone can handle up to 900 kq/s with <0.2% packet loss.
However, the drop to 600 kq/s does not seem to come from dnscol CPU usage but rather the packet capture
load on the kernel: the Knot speed drop is the same (to 600 kq/s) with dnscol cpulimited to just 0.5 CPU.




