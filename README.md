# Dns Collector

A super-fast traffic collector and pre-processor for DNS queries,
the first part of our DNS traffic analysis pipeline.

Released under [GNU GPL v3](https://www.gnu.org/licenses/) (see `LICENSE`).
Contact Tomáš Gavenčiak (gavento@ucw.cz) with any questions.

## Features

* Fast multithreaded processing (3 threads): up to 150 000 queries/s offline and offline (on i5 2.4 GHz, see benchmarks below).
* Matching requests to responses by (IPs, ports, transport, DNS ID), optionally also with QNAME. Matches the proposed [draft](https://tools.ietf.org/html/draft-ietf-dnsop-dns-capture-format-04#page-27).
* Reading capture files and live traces that [libtrace reads](http://www.wand.net.nz/trac/libtrace/wiki/SupportedTraceFormats), including kernel ringbuffer. Configurable packet filter.
* Pcap dumps of invalid packets with rate-limiting, compression and output file rotation.
* Configurable CBOR and CSV output (targeted at Impala/hadoop import, *NOT* RFC 4180 compatible) and optional binary CBOR output. Modular output allows easy implementation of other output formats.
* Automatic output file rotation and compression or other post-processing (any shell pipe command).
* Easy configuration via config files, periodic status and statistics logging, graceful shutdown on SIGINT.
* Well documented and clean code, C99, [GNU GPL v3](https://www.gnu.org/licenses/).

### Extracted DNS features

* Timing of request and response (microsecond accuracy)
* Network information (addresses, ports, protocols, TTL, packet and payload sizes)
* DNS header information (query information, id, flags, rcode, opcode, RR counts)
* EDNS information (version, extended flags and rcode, DAU, DHU, N3U, NSID)
* EDNS ping (NB: ID conflicts with DAU)

### Not implemented

* More EDNS features: Client subnet, other options (needs manual EDNS traversal).
* TCP flow recoinstruction for reused streams (currently only single-query TCP connections are supported).
* IP(4/6) fragmented packet reconstuction, DNS via ICMP. However, IP fragmentation and DNS via ICMP are uncommon.
* Possible improvement: Delay the responses in the workflow (cca 10 us) to remove capture timing jitter (when response is mistakenly seen before request).

## Building and running

Run `make` to compile the `dns-collector` binary. Use `USE_TCMALLOC=1 make` instead to build with tcmalloc.

Run `make docs` to generate developer Doxygen documentation in `docs/html`.

Linux packages are built in [project GitLab CI](https://gitlab.labs.nic.cz/labs/dns-collector/pipelines?scope=tags) and in [OpenBuildServece repo](https://build.opensuse.org/project/show/home:CZ-NIC:adam).

### Running

* `./dns-collector -i eth0 -oqueries.csv` runs a live trace with the default configuration. Ctrl+C (SIGINT) to gracefully stop. (See the config for automatical output rotation.)
* `./dns-collector -o queries.csv capture1.pcap capture2.pcap ...` processes the given pcaps in the order given.
* `./dns-collector -C dns-collector.conf` to use the given config file. Without any other parameters runs a live trace on the configured interface, may be combined with `-i` and `-o` overrides and offline pcap files as parameters.

See the distributed `dns-collector.conf` for more options and their meaning.

## Requirements

These are Ubuntu and Debian package names, but should be similar in other distros.

* Clang or GCC build environmrnt, make.
* `libtrace3-dev` 3.0.21+ (tested with 3.0.21 in Ubuntu Xenial to Artful, 3.0.18 from Ubuntu Trusty is not sufficient).
* `libknot7` and `libknot-dev` 2.6.x (tested with 2.6.4). Use [Knot repositories](https://www.knot-dns.cz/download/) for Debian and Ubuntu.

### Optional

* Optionally tcmalloc (from package `libgoogle-perftools-dev`, tested with ver 2.4 and 2.5) for faster allocation and cca 20% speedup (to use, compile with `USE_TCMALLOC=1 make`)

### Included

* [LibUCW](http://www.ucw.cz/libucw/) 6.5+ is included as a git subtree and built automatically.
* [TinyCBOR](https://github.com/intel/tinycbor) 0.5+ is included as a git subtree and built automatically.

## Memory footprint

On 64bit system, the basic memory usage is quite low (cca 30 MB). The only memory-intensive part is the matching window,
which keeps all the packets of the past matching window in memory. Consider this when choosing the matching window.

The total usage is therefore approximately: (30 + 3 * T * Q) MB, where T is the matching window in seconds, and Q the request frequency in kq/s (thousand queries per second).

For example, 10 s matching window and 10 kqps requires 300 MB. In an extremal situation, 10 s matching window and 150 kqps would require 5 GB of memory.

*NOTE:* The current memory usage is 2 kB per request+response packet pair, including parsed packets data, allocation overhead and matching hash table. This could be likely reduced to some 500 B (in the optimistic case of most requests being matched) by early extraction of output-relevant data from the packets before matching (now done on output), but that would complicate the code structure and limit extensibility.

## Benchmarks

*Testing machine:* `knot-master` 2x4 core Intel Xeon, 2.4 GHz. Debian 8.5. Online test packets are replayed from another machine via Intel 10G X520.

### Offline processing

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

## CBOR output

CBOR output has been introduced in 0.2 to adress some of the encoding problems of CSV: representing binary data and structured elements. [CBOR format](http://cbor.io/)
is defined in [RFC 7049](https://tools.ietf.org/html/rfc7049) and there are implementations for many common languages.

The CBOR output file is a sequence of independent CBOR items. This was preferred to a one large CBOR item per file to avoid having to load the entire file with most (non-streaming) parsers. The file structure resembles a CSV file with a header row:

The first item is an array of column names, corresponding to the present columns in order.
Each subsequent item is array of attributes for one record. The meanings and types of the attributes are the same as for the CSV files below. The only exceptions are EDNS data
with inner structure:

| field name | format |
| ---- | ---- |
| `req_edns_dau` | array of DAU numbers |
| `req_edns_dhu` | array of DHU numbers |
| `req_edns_n3u` | array of N3U numbers |
| `resp_edns_nsid` | raw NSID as a bytestring |
| `edns_client_subnet` | *TODO* |
| `edns_other` | *TODO* |

Compared to CSV, CBOR output uses cca 10% CPU (user time), is 30% smaller uncompressed and 5% smmaller gziped.

## CSV output

### Escaping

The output is primarily targetted for Impala import, so the CSV does NOT conform to RFC 4180.
Instead of quoting, problematic characters are escaped with a backslash. Every record is one line - all newlines are escaped.

The escaped characters are: backslash (`\\`), newline (`\n`), the configured field separator (e.g. `\,`),
all non-ASCII and most non-printable characters (`\ooo` with octal notation).

"NULL" values are represented by an empty string in numeric fields and by `\N` in string fields (as in Impala CSVs).

### Fields

The fields prefixed by `req_` always come from the request, `resp_` from the response.
The other fields may generally come from either request or response, depending on
request or response presence and other context. 

Some fields have a common configuration flag, for most the flag is the same as the field name. 

| field name   | config flag name | type | comment |
| ---- | ---- | ---- | ----- |
| `time`        | .         | DOUBLE  | seconds of request (or response if no request) since Unix epoch, microsecond precision |
| `delay_us`    | .         | INT     | request-response delay in microseconds  |
| `req_dns_len` | .         | INT     | DNS paload length |
| `resp_dns_len`| .         | INT     | DNS paload length     |
| `req_net_len` | .         | INT     | packet network length     |
| `resp_net_len`| .         | INT     | packet network length     |
| `client_addr` | .         | STRING  | e.g. "1.2.3.4" or "12:34::ef" |
| `client_port` | .         | INT     |      |
| `server_addr` | .         | STRING  | e.g. "1.2.3.4" or "12:34::ef" |
| `server_port` | .         | INT     |      |
| `net_proto`   | .         | INT     | 17 for UDP, 6 for TCP |
| `net_ipv`     | .         | INT     | 4 or 6 |
| `net_ttl`     | .         | INT     | TTL or hoplimit |
| `req_udp_sum` | .         | INT     | only if present (and not 0) |
| `id`          | .         | INT     | DNS ID |
| `qtype`       | .         | INT     |      |
| `qclass`      | .         | INT     |      |
| `opcode`      | .         | INT     |      |
| `rcode`       | .         | INT     |      |
| `resp_aa`     | `flags`   | BOOLEAN |      |
| `resp_tc`     | `flags`   | BOOLEAN |      |
| `req_rd`      | `flags`   | BOOLEAN |      |
| `resp_ra`     | `flags`   | BOOLEAN |      |
| `req_z`       | `flags`   | BOOLEAN |      |
| `resp_ad`     | `flags`   | BOOLEAN |      |
| `req_cd`      | `flags`   | BOOLEAN |      |
| `qname`       | .         | STRING  | including the final dot, escaped by libknot (most non-basic-DNS characters as `\ooo` in octal) |
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

### Impala and Entrada import

See [the wiki page on Impala and Entrada import](https://gitlab.labs.nic.cz/alan/dns-collector/wikis/import-to-impala) for more details
and SQL commands. Note that Impala can read gzip, lzo and bzip2 compressed CSV files transparently.

The table for all features (in the right order) is created by:

```sql
create table dnscol.csv_import (
  time DOUBLE, -- convert to timestamp
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
  resp_aa INT, -- convert to boolean
  resp_tc INT, -- convert to boolean
  req_rd INT, -- convert to boolean
  resp_ra INT, -- convert to boolean
  req_z INT, -- convert to boolean
  resp_ad INT, -- convert to boolean
  req_cd INT, -- convert to boolean
  qname STRING,
  resp_ancount INT,
  resp_arcount INT,
  resp_nscount INT,
  req_edns_ver INT,
  req_edns_udp INT,
  req_edns_do INT, -- convert to boolean
  resp_edns_rcode INT,
  req_edns_ping INT, -- convert to boolean
  req_edns_dau STRING,
  req_edns_dhu STRING,
  req_edns_n3u STRING,
  resp_edns_nsid STRING,
  edns_client_subnet STRING,
  edns_other STRING)
partitioned by (server STRING, batchname STRING)
ROW FORMAT DELIMITED FIELDS TERMINATED BY '|' ESCAPED BY '\\';
```

## Potential improvements

* More EDNS features
* Country/ASN detection (e.g. MaxMind integration?)
* TCP SYN/ACK drops (and then better TCP integration)

## Changelog

### 1.1 (2018-06-22)

* Better error messages and option defaults
* EDNS Ping support
* Option for QNAME matching
* Bugfixes, testsuite, CI tests
* Repo history cleanup, squash imported subtrees
* Add CI build and OBS repository

### 1.0 (2018-02-06)

* Improve multiple output types support
* Add CBOR output logging matching the CSV fields
* Add tinycbor as a git subtree
* Add simple CBOR-stream debugging utility
* Fix a flipped request/response bit bug

### 0.1.1 (2017-09-14)

* Fix backtrace output preventing core dump in upstream
* Updated distribution metadata and scripts, rename binaries

### 0.1 (2017-09-07)

* Initial release
