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
* `make` to compile the `dnscol` binary
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

## CSV output

### Escaping

The output is primarily targetted for Impala import, so the CSV does NOT conform to RFC 4180.
Instead of quoting, problematic characters are escaped with a backslash.

The escaped characters are: backslash (`\\`), newline (`\n`), the configured field separator (e.g. `\,`),
all non-ASCII and most non-printable characters (`\ooo` with octal notation).

SQL `NULL` values in numeric fields are represented by an empty field and by `\N` in string fields
(as in Impala).

### Fields

The fields prefixes by `req_` always come from the request, `resp_` from the response.
The other fields may generally come from either request or response, depending on
their presence and other context. All values, including DNS ID, are correctly
decoded from network endian.

| field name   | config flag name (if not same) | type | comment |
| ---- | ---- | ---- | ----- |
| `time`        |           | DOUBLE  | seconds of request (or response if no request) since Unix epoch, microsecond precision |
| `delay_us`    |           | INT     | request-response delay in microseconds  |
| `req_dns_len` |           | INT     | DNS paload length |
| `resp_dns_len`|           | INT     | DNS paload length     |
| `req_net_len` |           | INT     | Packet network length     |
| `resp_net_len`|           | INT     | Packet network length     |
| `client_addr` |           | STRING  | e.g. "1.2.3.4" or "12:34::ef" |
| `client_port` |           | INT     |      |
| `server_addr` |           | STRING  | e.g. "1.2.3.4" or "12:34::ef" |
| `server_port` |           | INT     |      |
| `net_proto`   |           | INT     | 17 for UDP, 6 for TCP |
| `net_ipv`     |           | INT     | 4 or 6 |
| `net_ttl`     |           | INT     | TTL or hoplimit |
| `req_udp_sum` |           | INT     | only if present (and not 0) |
| `id`          |           | INT     | DNS ID |
| `qtype`       |           | INT     |      |
| `qclass`      |           | INT     |      |
| `opcode`      |           | INT     |      |
| `rcode`       |           | INT     |      |
| `resp_aa`     | `flags`   | BOOLEAN |      |
| `resp_tc`     | `flags`   | BOOLEAN |      |
| `req_rd`      | `flags`   | BOOLEAN |      |
| `resp_ra`     | `flags`   | BOOLEAN |      |
| `req_z`       | `flags`   | BOOLEAN |      |
| `resp_ad`     | `flags`   | BOOLEAN |      |
| `req_cd`      | `flags`   | BOOLEAN |      |
| `qname`       |           | STRING  | including the final dot, escaped by libknot (most non-basic-DNS characters as `\ooo` in octal) |
| `resp_ancount`|`rr_counts`| INT     |      |
| `resp_arcount`|`rr_counts`| INT     |      |
| `resp_nscount`|`rr_counts`| INT     |      |
| `req_edns_ver`| `edns`    | INT     | version, distinguish 0 and `NULL`! |
| `req_edns_udp`| `edns`    | INT     |      |
| `req_edns_do` | `edns`    | BOOLEAN |      |
| `resp_edns_rcode`|`edns`  | INT     | extended part of `rcode` |
| `req_edns_ping`|`edns`    | BOOLEAN |      |
| `req_edns_dau`| `edns`    | STRING  | comma-separated numbers, e.g. "1,3,5" |
| `req_edns_dhu`| `edns`    | STRING  | comma-separated numbers, e.g. "1,3,5" |
| `req_edns_n3u`| `edns`    | STRING  | comma-separated numbers, e.g. "1,3,5" |
| `resp_edns_nsid`|`edns`   | STRING  | escaped NSID |
| `edns_client_subnet`|`edns`| STRING | *TODO* |
| `edns_other`  | `edns`    | STRING  | *TODO* |

### Impala import

It is recommended to create the Impala table as (with the selected field separator):
```sql
CREATE TABLE table_name ( ... )
ROW FORMAT DELIMITED FIELDS TERMINATED BY '|' ESCAPED BY '\\';
```

The table for all features (in the right order) is created by:
```sql
CREATE TABLE dnscol_csv_import (
time TIMESTAMP, -- has nanosecond precision in Impala
delay_us INT,
req_dns_len INT,
resp_dns_len INT,
req_net_len INT,
resp_net_len INT,
client_addr STRING,
client_port INT,
server_addr STRING,
server_port INT,
net_proto INT,
net_ipv INT,
net_ttl INT,
req_udp_sum INT,
id INT,
qtype INT,
qclass INT,
opcode INT,
rcode INT,
resp_aa BOOLEAN,
resp_tc BOOLEAN,
req_rd BOOLEAN,
resp_ra BOOLEAN,
req_z BOOLEAN,
resp_ad BOOLEAN,
req_cd BOOLEAN,
qname STRING,
resp_ancount INT,
resp_arcount INT,
resp_nscount INT,
req_edns_ver INT,
req_edns_udp INT,
req_edns_do BOOLEAN,
resp_edns_rcode INT,
req_edns_ping BOOLEAN,
req_edns_dau STRING,
req_edns_dhu STRING,
req_edns_n3u STRING,
resp_edns_nsid STRING,
edns_client_subnet STRING,
edns_other STRING)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '|' ESCAPED BY '\\';
```

Optionally, you might want to add `PARTITIONED BY (server STRING)` or even a date-based partitioning.
Note that Impala can read gzip, lzo and bzip2 compressed CSV files transparently.
