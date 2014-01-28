/*
 *	UCW Library -- Configuration files: sections
 *
 *	(c) 2001--2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2003--2014 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/conf.h>
#include <ucw/conf-internal.h>
#include <ucw/clists.h>
#include <ucw/binsearch.h>
#include <ucw/gary.h>

#include <string.h>

/* Dirty sections */

void
cf_add_dirty(struct cf_section *sec, void *ptr)
{
  struct cf_context *cc = cf_get_context();
  dirtsec_grow(&cc->dirty, cc->dirties+1);
  struct dirty_section *dest = cc->dirty.ptr + cc->dirties;
  if (cc->dirties && dest[-1].sec == sec && dest[-1].ptr == ptr)
    return;
  dest->sec = sec;
  dest->ptr = ptr;
  cc->dirties++;
}

#define ASORT_PREFIX(x)	dirtsec_##x
#define ASORT_KEY_TYPE	struct dirty_section
#define ASORT_LT(x,y)	x.sec < y.sec || x.sec == y.sec && x.ptr < y.ptr
#include <ucw/sorter/array-simple.h>

static void
sort_dirty(struct cf_context *cc)
{
  if (cc->dirties <= 1)
    return;
  dirtsec_sort(cc->dirty.ptr, cc->dirties);
  // and compress the list
  struct dirty_section *read = cc->dirty.ptr + 1, *write = cc->dirty.ptr + 1, *limit = cc->dirty.ptr + cc->dirties;
  while (read < limit) {
    if (read->sec != read[-1].sec || read->ptr != read[-1].ptr) {
      if (read != write)
	*write = *read;
      write++;
    }
    read++;
  }
  cc->dirties = write - cc->dirty.ptr;
}

/* Initialization */

struct cf_item *
cf_find_subitem(struct cf_section *sec, const char *name)
{
  struct cf_item *ci = sec->cfg;
  for (; ci->cls; ci++)
    if (!strcasecmp(ci->name, name))
      return ci;
  return ci;
}

static void
inspect_section(struct cf_section *sec)
{
  sec->flags = 0;
  struct cf_item *ci;
  for (ci=sec->cfg; ci->cls; ci++)
    if (ci->cls == CC_SECTION) {
      inspect_section(ci->u.sec);
      sec->flags |= ci->u.sec->flags & (SEC_FLAG_DYNAMIC | SEC_FLAG_CANT_COPY);
    } else if (ci->cls == CC_LIST) {
      inspect_section(ci->u.sec);
      sec->flags |= SEC_FLAG_DYNAMIC | SEC_FLAG_CANT_COPY;
    } else if (ci->cls == CC_DYNAMIC || ci->cls == CC_BITMAP)
      sec->flags |= SEC_FLAG_DYNAMIC;
    else if (ci->cls == CC_PARSER) {
      sec->flags |= SEC_FLAG_CANT_COPY;
      if (ci->number < 0)
	sec->flags |= SEC_FLAG_DYNAMIC;
    }
  if (sec->copy)
    sec->flags &= ~SEC_FLAG_CANT_COPY;
  sec->flags |= ci - sec->cfg;		// record the number of entries
}

void
cf_declare_rel_section(const char *name, struct cf_section *sec, void *ptr, uns allow_unknown)
{
  struct cf_context *cc = cf_obtain_context();
  if (!cc->sections.cfg)
  {
    cc->sections.size = 50;
    cc->sections.cfg = xmalloc_zero(cc->sections.size * sizeof(struct cf_item));
  }
  struct cf_item *ci = cf_find_subitem(&cc->sections, name);
  if (ci->cls)
    die("Cannot register section %s twice", name);
  ci->cls = CC_SECTION;
  ci->name = name;
  ci->number = 1;
  ci->ptr = ptr;
  ci->u.sec = sec;
  inspect_section(sec);
  if (allow_unknown)
    sec->flags |= SEC_FLAG_UNKNOWN;
  ci++;
  if (ci - cc->sections.cfg >= (int) cc->sections.size)
  {
    cc->sections.cfg = xrealloc(cc->sections.cfg, 2*cc->sections.size * sizeof(struct cf_item));
    bzero(cc->sections.cfg + cc->sections.size, cc->sections.size * sizeof(struct cf_item));
    cc->sections.size *= 2;
  }
}

void
cf_declare_section(const char *name, struct cf_section *sec, uns allow_unknown)
{
  cf_declare_rel_section(name, sec, NULL, allow_unknown);
}

void
cf_init_section(const char *name, struct cf_section *sec, void *ptr, uns do_bzero)
{
  if (do_bzero) {
    ASSERT(sec->size);
    bzero(ptr, sec->size);
  }
  for (struct cf_item *ci=sec->cfg; ci->cls; ci++)
    if (ci->cls == CC_SECTION)
      cf_init_section(ci->name, ci->u.sec, ptr + (uintptr_t) ci->ptr, 0);
    else if (ci->cls == CC_LIST)
      clist_init(ptr + (uintptr_t) ci->ptr);
    else if (ci->cls == CC_DYNAMIC) {
      void **dyn = ptr + (uintptr_t) ci->ptr;
      if (!*dyn)			// replace NULL by an empty array
	*dyn = GARY_FOREVER_EMPTY;
    }
  if (sec->init) {
    char *msg = sec->init(ptr);
    if (msg)
      die("Cannot initialize section %s: %s", name, msg);
  }
}

static char *
commit_section(struct cf_section *sec, void *ptr, uns commit_all)
{
  struct cf_context *cc = cf_get_context();
  char *err;

  for (struct cf_item *ci=sec->cfg; ci->cls; ci++)
    if (ci->cls == CC_SECTION) {
      if ((err = commit_section(ci->u.sec, ptr + (uintptr_t) ci->ptr, commit_all))) {
	msg(L_ERROR, "Cannot commit section %s: %s", ci->name, err);
	return "commit of a subsection failed";
      }
    } else if (ci->cls == CC_LIST) {
      uns idx = 0;
      CLIST_FOR_EACH(cnode *, n, * (clist*) (ptr + (uintptr_t) ci->ptr))
	if (idx++, err = commit_section(ci->u.sec, n, commit_all)) {
	  msg(L_ERROR, "Cannot commit node #%d of list %s: %s", idx, ci->name, err);
	  return "commit of a list failed";
	}
    }
  if (sec->commit) {
    /* We have to process the whole tree of sections even if just a few changes
     * have been made, because there are dependencies between commit-hooks and
     * hence we need to call them in a fixed order.  */
#define ARY_LT_X(ary,i,x) ary[i].sec < x.sec || ary[i].sec == x.sec && ary[i].ptr < x.ptr
    struct dirty_section comp = { sec, ptr };
    uns pos = BIN_SEARCH_FIRST_GE_CMP(cc->dirty.ptr, cc->dirties, comp, ARY_LT_X);

    if (commit_all
	|| (pos < cc->dirties && cc->dirty.ptr[pos].sec == sec && cc->dirty.ptr[pos].ptr == ptr))
      return sec->commit(ptr);
  }
  return 0;
}

int
cf_commit_all(enum cf_commit_mode cm)
{
  struct cf_context *cc = cf_get_context();
  sort_dirty(cc);
  if (cm == CF_NO_COMMIT)
    return 0;
  if (commit_section(&cc->sections, NULL, cm == CF_COMMIT_ALL))
    return 1;
  cc->dirties = 0;
  return 0;
}
