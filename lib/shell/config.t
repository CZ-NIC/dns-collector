# Tests for configuration parser

Run:	obj/lib/shell/config -C/dev/null -S 'sec1{int1=23; long1=1234567812345678; long2=4321; str1="s1"; str2="s2"}' 'sec1 {#int1; ##long1; -str1; str2; #int2=123; ##long2=1234}; sec2{str3}'
Out:	CF_sec1_int1='23'
	CF_sec1_long1='1234567812345678'
	CF_sec1_str2='s2'
	CF_sec1_int2='123'
	CF_sec1_long2='4321'
	CF_sec2_str3=''

Run:	obj/lib/shell/config -C/dev/null -S 'sec1{list1 1 a1 b1; list1:clear; list1 2 a2 b2 3 a3 b3}' 'sec1 {@list1 {#int1; str1; -str2}}'
Out:	CF_sec1_list1_int1[0]='2'
	CF_sec1_list1_str1[0]='a2'
	CF_sec1_list1_int1[1]='3'
	CF_sec1_list1_str1[1]='a3'

Run:	obj/lib/shell/config -C/dev/null -S 'sec1{list1 {str1=1; list2=a b c}; list1 {str1=2; list2=d e}}' 'sec1 {@list1 {str1; @list2{str2}}}'
Out:	CF_sec1_list1_str1[0]='1'
	CF_sec1_list1_list2_str2[0]='a'
	CF_sec1_list1_list2_str2[1]='b'
	CF_sec1_list1_list2_str2[2]='c'
	CF_sec1_list1_str1[1]='2'
	CF_sec1_list1_list2_str2[3]='d'
	CF_sec1_list1_list2_str2[4]='e'

Run:	obj/lib/shell/config -C/dev/null 'sec{str=a'\''b"c'\''d"\\e'\''f"g}'
Out:	CF_sec_str='ab"cd\e'\''fg'
