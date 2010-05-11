# Test for the strtonum module

## Testing str_to_uns(.) (It is supoposed that uns is 4 bytes integer)

# 1
Run:	../obj/ucw/strtonum-test '0o+-_' '0o100_000xc' | grep '^i'
Out:	i32768

# 2
Run:	../obj/ucw/strtonum-test '0XoB+-_' '4_294_967_295xcyv' | grep '^i'
Out:	i4294967295

# 3
Run:	../obj/ucw/strtonum-test '0XoB+-_' '4_294_967_296xc' | grep '^e'
Out:	e[Numeric overflow]

# 4
Run:	../obj/ucw/strtonum-test '0XoBt+-_' '4_294_967_296xc' | grep '^h'
Out:	hffffffff

# 5
Run:	../obj/ucw/strtonum-test '0XoBt+-_' '4_294_967_296xc' | grep '^b'
Out:	b13:0x78

# 6
Run:	../obj/ucw/strtonum-test '0XoBt+-_' '0x__4_' | grep '^i'
Out:	i4

# 7
Run:	../obj/ucw/strtonum-test '0XoBt+-_' '0x__4_' | grep '^b'
Out:	b6:0x0

# 8
Run:	../obj/ucw/strtonum-test '0XoBt+-_' '0xXW' | grep '^e'
Out:	e[Number contains no digits]

# 9
Run:	../obj/ucw/strtonum-test '0XoBt+-_' '0xXW' | grep '^b'
Out:	b2:0x58

# 10
Run:	../obj/ucw/strtonum-test '0oBt+-_' '0xXW' | grep '^i'
Out:	i0

# 11
Run:	../obj/ucw/strtonum-test '0oBt+-_' '0xXW' | grep '^b'
Out:	b1:0x78

# 12
Run:	../obj/ucw/strtonum-test '0oBt+-_Z' '0xXW' | grep '^e'
Out:	e[Invalid character]

# 13
Run:	../obj/ucw/strtonum-test '0oBt+-_Z' '0xXW' | grep '^b'
Out:	b1:0x78

# 14
Run:	../obj/ucw/strtonum-test '0Bs+-_' '2_147_483_647xxx' | grep '^i'
Out:	i2147483647

# 15
Run:	../obj/ucw/strtonum-test '0Bs+-_' '2_147_483_647xxx' | grep '^i'
Out:	i2147483647

# 16
Run:	../obj/ucw/strtonum-test '0Bs+-_' '2_147_483_648xxx' | grep '^e'
Out:	e[Numeric overflow]

# 17
Run:	../obj/ucw/strtonum-test '0Bs+-_' '-2_147_483_648xxx' | grep '^i'
Out:	i-2147483648

# 18
Run:	../obj/ucw/strtonum-test '0Bs+-_' '-2_147_483_649xxx' | grep '^e'
Out:	e[Numeric overflow]

# 19
Run:	../obj/ucw/strtonum-test '0Bts+-_' '2_147_483_648xxx' | grep '^i'
Out:	i2147483647

# 20
Run:	../obj/ucw/strtonum-test '0Bts+-_' '-2_147_483_649xxx' | grep '^i'
Out:	i-2147483648

# 21
Run:	../obj/ucw/strtonum-test '0Bts+-_' '-2_147_483_649xxx' | grep '^i'
Out:	i-2147483648

# 22
Run:	../obj/ucw/strtonum-test '0X+-' '0xABCDEFxxx' | grep '^h'
Out:	habcdef

# 23
Run:	../obj/ucw/strtonum-test '0X+-_' '0x00_AB_CD_EFxxx' | grep '^h'
Out:	habcdef

# 24
Run:	../obj/ucw/strtonum-test '0Xs+-_' '-0x00AB_CDEFxxx' | grep '^h'
Out:	hff543211

# 25
Run:	../obj/ucw/strtonum-test '0XBs+-_' '-0x00AB_CDEFxxx' | grep '^h'
Out:	hff543211

# 26
Run:	../obj/ucw/strtonum-test '0B+-_' '0B1111_0000_1000_0101_1010xxx' | grep '^h'
Out:	hf085a

# 27
Run:	../obj/ucw/strtonum-test '0+-_' '0B1111_0000_1000_0101_1010xxx' | grep '^b'
Out:	b1:0x42

# 28
Run:	../obj/ucw/strtonum-test '0o+-_' '0o70105xxx' | grep '^i'
Out:	i28741

# 29
Run:	../obj/ucw/strtonum-test '0os+-_' '-0o70105xxx' | grep '^i'
Out:	i-28741

# 30
Run:	../obj/ucw/strtonum-test '0os+-_' '-0o___________xxx' | grep '^e'
Out:	e[Number contains no digits]

# 31
Run:	../obj/ucw/strtonum-test '2+-_' '10578ABCG' | grep '^i'
Out:	i2

# 32
Run:	../obj/ucw/strtonum-test '2s+-_' '-10578ABCG' | grep '^i'
Out:	i-2

# 33
Run:	../obj/ucw/strtonum-test '8+-_' '10578ABCG' | grep '^i'
Out:	i559

# 34
Run:	../obj/ucw/strtonum-test '8s+-_' '-10578ABCG' | grep '^i'
Out:	i-559

# 35
Run:	../obj/ucw/strtonum-test '0+-_' '10578ABCG' | grep '^i'
Out:	i10578

# 36
Run:	../obj/ucw/strtonum-test '0s+-_' '-10578ABCG' | grep '^i'
Out:	i-10578

# 37
Run:	../obj/ucw/strtonum-test 'h+-_' '10578ABCG' | grep '^i'
Out:	i274172604

# 38
Run:	../obj/ucw/strtonum-test 'hs+-_' '-10578ABCG' | grep '^i'
Out:	i-274172604

## Testing str_to_uintmax(.) (It is supoposed that uintmax_t is 8 bytes integer)
# 39
Run:	../obj/ucw/strtonum-test 'h+-_' 'FFFF_FFFF_ffff_ffFF' | grep '^H'
Out:	Hffffffffffffffff

# 40
Run:	../obj/ucw/strtonum-test 'h+-_' 'FFFF_FFFF_ffff_ffFF' | grep '^I'
Out:	I18446744073709551615

#41
Run:	../obj/ucw/strtonum-test '0t+-_' '1844674407370000009551616' | grep '^I'
Out:	I18446744073709551615

#42
Run:	../obj/ucw/strtonum-test '0+-_' '18446744073709551616' | grep '^E'
Out:	E[Numeric overflow]

# 43
Run:	../obj/ucw/strtonum-test '0+-_' '18446744073709551614' | grep '^H'
Out:	Hfffffffffffffffe

# 44
Run:	../obj/ucw/strtonum-test '0s+-_' '9223372036854775807L' | grep '^I'
Out:	I9223372036854775807

# 45
Run:	../obj/ucw/strtonum-test '0s+-_' '9223372036854775806L' | grep '^I'
Out:	I9223372036854775806

# 46
Run:	../obj/ucw/strtonum-test '0st+-_' '92233720368547758000000L' | grep '^I'
Out:	I9223372036854775807

# 47
Run:	../obj/ucw/strtonum-test '0s+-_' '9223372036854775808L' | grep '^E'
Out:	E[Numeric overflow]

# 48
Run:	../obj/ucw/strtonum-test '0s+-_' '-9223372036854775808L' | grep '^I'
Out:	I-9223372036854775808

# 49
Run:	../obj/ucw/strtonum-test '0s+-_' '-9223372036854775807L' | grep '^I'
Out:	I-9223372036854775807

# 50
Run:	../obj/ucw/strtonum-test '0st+-_' '-9223372036854775800000L' | grep '^I'
Out:	I-9223372036854775808

# 51
Run:	../obj/ucw/strtonum-test '0s+-_' '-9223372036854775809L' | grep '^E'
Out:	E[Numeric overflow]

