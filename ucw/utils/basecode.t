# Tests for base64 and base224 modules

Name:	Base64 encode
Run:	../obj/ucw/utils/basecode -e
In:	Here are some test data
Out:	SGVyZSBhcmUgc29tZSB0ZXN0IGRhdGEK

Name:	Base64 decode
Run:	../obj/ucw/utils/basecode -d
In:	SGVyZSBhcmUgc29tZSB0ZXN0IGRhdGEK
Out:	Here are some test data

Name:	Base224 encode & decode
Run:	../obj/ucw/utils/basecode -E | ../obj/ucw/utils/basecode -D
In:	Some more test data for 224 encoding
Out:	Some more test data for 224 encoding
