| entrada col   | type		| dnscol field   |CVS| comment |
| ------------- | ------------- | -------------- |---| ------- |
| *Timing* |||||
| unixtime 	| BIGINT	| timestamp      | X | Seconds since Epoch
| time 		| BIGINT	| timestamp      | X | Miliseconds since Epoch 
| year		| INT		| timestamp      | X |  |
| month		| INT		| timestamp      | X |  |
| day		| INT		| timestamp      | X |  |
| time_micro	| BIGINT	| delay_us       | X | Processing time (reply-request) |
| *Lengths* |||||
| len 		| INT		| req_pkt_len    | X | Request pkt length |
| res_len	| INT		| resp_pkt_len   | X | Response pkt length |
| dns_len 	| INT		| req_dns_len    | X | DNS payload length  |
| dns_res_len	| INT		| resp_dns_len   | X | DNS reply payload length |
| *Net features* |||||
| src 		| STRING	| client_addr    | X | Source (client) IP  |
| srcp 		| INT		| client_port    | X | Source (client) port  |
| dst 		| STRING	| server_addr    | X | Dest. (server) IP  |
| dstp 		| INT		| server_port    | X | Dest. (server) port  |
| ttl 		| INT		| req_ttl        | X | Request TTL
| ipv 		| INT		| net_ipv        | X | IPver: 4 / 6
| prot 		| INT		| net_proto      | X | TCP/UDP/... (val such as "17" =UDP)  |
| udp_sum 	| INT		| req_udp_sum    | X | UDP checksum  |
| *DNS core* |||||
| id 		| INT		| id             | X | DNS ID
| qname 	| STRING	| qname          | X | full qname, dotted with final dot
| domainname 	| STRING	| qname          | X | last two domains (or TLD + 1 label), no final dot
| labels	| INT		| qname          | X |  |
| aa 		| BOOLEAN	| resp_aa        | X | Response (or request if no response) flags |
| tc 		| BOOLEAN	| resp_tc        | X |  |
| rd 		| BOOLEAN	| req_rd         | X |  |
| ra 		| BOOLEAN	| resp_ra        | X |  |
| z 		| BOOLEAN	| req_z          | X |  |
| ad 		| BOOLEAN	| resp_ad        | X |  |
| cd		| BOOLEAN	| req_cd         | X |  |
| ancount	| INT		| resp_ancount   | X |  |
| arcount	| INT		| resp_arcount   | X |  |
| nscount	| INT		| resp_nscount   | X |  |
| qdcount	| INT		| NA             | - | Only 0 or 1, determined by qname |
| opcode	| INT		| opcode         | X | Request opcode (=response) |
| rcode		| INT		| rcode          | X | Response code  |
| qtype		| INT		| qtype          | X |  |
| qclass	| INT		| qclass         | X |  |
| *EDNS* |||||
| edns_udp	| INT		| edns_udp       | X | UDP payload |
| edns_version	| SMALLINT	| edns_version   | X | Version (NULL or 0)  |
| edns_do	| BOOLEAN	| edns_do        | X | DO bit |
| edns_ping	| BOOLEAN	| edns_ping      |   | Tough to detect! See [here](https://github.com/SIDN/entrada/blob/b787af190267df148683151638ce94508bd6139e/dnslib4java/src/main/java/nl/sidn/dnslib/message/records/edns0/OPTResourceRecord.java#L146) |
| edns_nsid	| STRING	| edns_nsid      |   |  |
| edns_dnssec_dau| STRING	| edns_dnssec_dau| X | Comma-separated list "1,3,5" |
| edns_dnssec_dhu| STRING	| edns_dnssec_dhu| X | Comma-separated list "1,3,5" |
| edns_dnssec_n3u| STRING	| edns_dnssec_n3u| X | Comma-separated list "1,3,5" |
| edns_client_subnet| STRING	|                |   | "4,118.71.70/24,0" or "4,62.168.119/24,0" via `(fam == 1? "4,": "6,") + address + "/" + sourcenetmask + "," + scopenetmask;` |
| edns_other	| STRING	|                |   | List of EDNS option types like "3,10,9" |
| edns_client_subnet_asn|STRING | NA             | - | By IP list (Maxmind) |
| edns_client_subnet_country|STRING| NA          | - | By IP list (Maxmind) |
| -             |               | edns_ext_rcode | X | Not implemented by Entrada |
| *Misc and unknown* |||||
| country	| STRING	| NA             | - | 2 letter code ("CZ", ..)  |
| asn		| STRING	| NA             | - | ASN ("AS1234", ...)  |
| frag 		| INT		| NULL           | - | Fragmentation? (Entrada: all NULL)
| resp_frag	| INT		| NULL           | - | Unknown (Entrada: all NULL) |
| proc_time	| INT		| NULL           | - | Unknown (Entrada: all NULL)  |
| is_google	| BOOLEAN	| NA             | - | By IP list (Maxmind) |
| is_opendns	| BOOLEAN	| NA             | - | By IP list (Maxmind) |
| server_location| STRING 	| NULL           | - | Server location (Entrada: all NULL) |
| server	| STRING	| NA             | - |  |
