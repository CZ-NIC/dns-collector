#ifndef _IMAGES_MATH_H
#define _IMAGES_MATH_H

#ifdef CONFIG_UCW_CLEAN_ABI
#define fast_div_tab ucw_fast_div_tab
#define fast_sqrt_tab ucw_fast_sqrt_tab
#endif

extern const u32 fast_div_tab[];
extern const byte fast_sqrt_tab[];

static inline uns isqr(int x)
{
  return x * x;
}

static inline uns fast_div_u32_u8(uns x, uns y)
{
  return ((u64)(x) * fast_div_tab[y]) >> 32;
}

static inline uns fast_sqrt_u16(uns x)
{
  uns y;
  if (x < (1 << 10) - 3)
    y = fast_sqrt_tab[(x + 3) >> 2] >> 3;
  else if (x < (1 << 14) - 28)
    y = fast_sqrt_tab[(x + 28) >> 6] >> 1;
  else
    y = fast_sqrt_tab[x >> 8];
  return (x < y * y) ? y - 1 : y;
}

static inline uns fast_sqrt_u32(uns x)
{
  uns y;
  if (x < (1 << 16))
    {
      if (x < (1 << 10) - 3)
  	y = fast_sqrt_tab[(x + 3) >> 2] >> 3;
      else if (x < (1 << 14) - 28)
	y = fast_sqrt_tab[(x + 28) >> 6] >> 1;
      else
        y = fast_sqrt_tab[x >> 8];
    }
  else
    {
      if (x < (1 << 24))
        {
	  if (x < (1 << 20))
	    {
              y = fast_sqrt_tab[x >> 12];
              y = (fast_div_u32_u8(x, y) >> 3) + (y << 1);
            }
	  else
	    {
              y = fast_sqrt_tab[x >> 16];
              y = (fast_div_u32_u8(x, y) >> 5) + (y << 3);
            }
        }
      else
        {
          if (x < (1 << 28))
	    {
              if (x < (1 << 26))
	        {
                  y = fast_sqrt_tab[x >> 18];
                  y = (fast_div_u32_u8(x, y) >> 6) + (y << 4);
                }
	      else
	        {
                  y = fast_sqrt_tab[x >> 20];
                  y = (fast_div_u32_u8(x, y) >> 7) + (y << 5);
                }
            }
	  else
	    {
              if (x < (1 << 30))
	        {
                  y = fast_sqrt_tab[x >> 22];
                  y = (fast_div_u32_u8(x, y) >> 8) + (y << 6);
                }
	      else
	        {
                  y = fast_sqrt_tab[x >> 24];
                  y = (fast_div_u32_u8(x, y) >> 9) + (y << 7);
                }
            }
        }
    }
  return (x < y * y) ? y - 1 : y;
}

#endif
