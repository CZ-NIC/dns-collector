/*
 *	UCW Library -- Mapping of File Parts
 *
 *	(c) 2003--2006 Martin Mares <mj@ucw.cz>
 *	(c) 2003--2005 Robert Spalek <robert@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#ifndef _UCW_PARTMAP_H
#define _UCW_PARTMAP_H

#ifdef CONFIG_UCW_CLEAN_ABI
#define partmap_close ucw_partmap_close
#define partmap_load ucw_partmap_load
#define partmap_open ucw_partmap_open
#define partmap_size ucw_partmap_size
#endif

struct partmap {
  int fd;
  ucw_off_t file_size;
  ucw_off_t start_off, end_off;
  byte *start_map;
  int writeable;
};

struct partmap *partmap_open(char *name, int writeable);
void partmap_close(struct partmap *p);
ucw_off_t partmap_size(struct partmap *p);
void partmap_load(struct partmap *p, ucw_off_t start, uint size);

static inline void *partmap_map(struct partmap *p, ucw_off_t start, uint size UNUSED)
{
#ifndef CONFIG_UCW_PARTMAP_IS_MMAP
  if (unlikely(!p->start_map || start < p->start_off || (ucw_off_t) (start+size) > p->end_off))
    partmap_load(p, start, size);
#endif
  return p->start_map + (start - p->start_off);
}

static inline void *partmap_map_forward(struct partmap *p, ucw_off_t start, uint size UNUSED)
{
#ifndef CONFIG_UCW_PARTMAP_IS_MMAP
  if (unlikely((ucw_off_t) (start+size) > p->end_off))
    partmap_load(p, start, size);
#endif
  return p->start_map + (start - p->start_off);
}

#endif
