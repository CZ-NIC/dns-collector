# A sed script to weed out private symbols from <ucw/autoconf.h>

/^\//n

# Excluded symbols (danger of collision)
/^#define CONFIG_DEBUG$/d

# Included symbols
/^#define CONFIG_/n
/^#define CPU_/n

d
