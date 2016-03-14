# Dns Collector

## Requirements

* Clang or GCC, make
* Libraries: `apt-get install liblz4-dev libpcap-dev libtrace3-dev` 
* Protocol Buffer libs (optional): `apt-get install protobuf-c-compiler protobuf-compiler libprotobuf-c-dev python-protobuf`

## Installation and usage

Run:
* `make` to compile the libs and dnscol
* `make docs` to get documentation in `docs/html`
* `src/dnscol -C dnscol.conf pcap_files ...` to process captures offline

