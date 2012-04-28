/*
 *	UCW Library -- Configuration files: parsing input streams
 *
 *	(c) 2001--2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2003--2012 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/conf.h>
#include <ucw/conf-internal.h>
#include <ucw/clists.h>
#include <ucw/mempool.h>
#include <ucw/fastbuf.h>
#include <ucw/chartype.h>
#include <ucw/string.h>
#include <ucw/stkstring.h>

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

/* Text file parser */

#define MAX_LINE	4096

#include <ucw/bbuf.h>

#define GBUF_TYPE	uns
#define GBUF_PREFIX(x)	split_##x
#include <ucw/gbuf.h>

struct cf_parser_state {
  const char *name_parse_fb;
  struct fastbuf *parse_fb;
  uns line_num;
  char *line;
  split_t word_buf;
  uns words;
  uns ends_by_brace;		// the line is ended by "{"
  bb_t copy_buf;
  uns copied;
  char line_buf[];
};

static int
get_line(struct cf_parser_state *p, char **msg)
{
  int err = bgets_nodie(p->parse_fb, p->line_buf, MAX_LINE);
  p->line_num++;
  if (err <= 0) {
    *msg = err < 0 ? "Line too long" : NULL;
    return 0;
  }
  p->line = p->line_buf;
  while (Cblank(*p->line))
    p->line++;
  return 1;
}

static void
append(struct cf_parser_state *p, char *start, char *end)
{
  uns len = end - start;
  bb_grow(&p->copy_buf, p->copied + len + 1);
  memcpy(p->copy_buf.ptr + p->copied, start, len);
  p->copied += len + 1;
  p->copy_buf.ptr[p->copied-1] = 0;
}

static char *
get_word(struct cf_parser_state *p, uns is_command_name)
{
  char *msg;
  char *line = p->line;

  if (*line == '\'') {
    line++;
    while (1) {
      char *start = line;
      while (*line && *line != '\'')
	line++;
      append(p, start, line);
      if (*line)
	break;
      p->copy_buf.ptr[p->copied-1] = '\n';
      if (!get_line(p, &msg))
	return msg ? : "Unterminated apostrophe word at the end";
      line = p->line;
    }
    line++;

  } else if (*line == '"') {
    line++;
    uns start_copy = p->copied;
    while (1) {
      char *start = line;
      uns escape = 0;
      while (*line) {
	if (*line == '"' && !escape)
	  break;
	else if (*line == '\\')
	  escape ^= 1;
	else
	  escape = 0;
	line++;
      }
      append(p, start, line);
      if (*line)
	break;
      if (!escape)
	p->copy_buf.ptr[p->copied-1] = '\n';
      else // merge two lines
	p->copied -= 2;
      if (!get_line(p, &msg))
	return msg ? : "Unterminated quoted word at the end";
      line = p->line;
    }
    line++;

    char *tmp = stk_str_unesc(p->copy_buf.ptr + start_copy);
    uns l = strlen(tmp);
    bb_grow(&p->copy_buf, start_copy + l + 1);
    strcpy(p->copy_buf.ptr + start_copy, tmp);
    p->copied = start_copy + l + 1;

  } else {
    // promised that *line is non-null and non-blank
    char *start = line;
    while (*line && !Cblank(*line)
	&& *line != '{' && *line != '}' && *line != ';'
	&& (*line != '=' || !is_command_name))
      line++;
    if (*line == '=') {				// nice for setting from a command-line
      if (line == start)
	return "Assignment without a variable";
      *line = ' ';
    }
    if (line == start)				// already the first char is control
      line++;
    append(p, start, line);
  }
  while (Cblank(*line))
    line++;
  p->line = line;
  return NULL;
}

static char *
get_token(struct cf_parser_state *p, uns is_command_name, char **err)
{
  *err = NULL;
  while (1) {
    if (!*p->line || *p->line == '#') {
      if (!is_command_name || !get_line(p, err))
	return NULL;
    } else if (*p->line == ';') {
      *err = get_word(p, 0);
      if (!is_command_name || *err)
	return NULL;
    } else if (*p->line == '\\' && !p->line[1]) {
      if (!get_line(p, err)) {
	if (!*err)
	  *err = "Last line ends by a backslash";
	return NULL;
      }
      if (!*p->line || *p->line == '#')
	msg(L_WARN, "The line %s:%d following a backslash is empty", p->name_parse_fb ? : "", p->line_num);
    } else {
      split_grow(&p->word_buf, p->words+1);
      uns start = p->copied;
      p->word_buf.ptr[p->words++] = p->copied;
      *err = get_word(p, is_command_name);
      return *err ? NULL : p->copy_buf.ptr + start;
    }
  }
}

static char *
split_command(struct cf_parser_state *p)
{
  p->words = p->copied = p->ends_by_brace = 0;
  char *msg, *start_word;
  if (!(start_word = get_token(p, 1, &msg)))
    return msg;
  if (*start_word == '{')			// only one opening brace
    return "Unexpected opening brace";
  while (*p->line != '}')			// stays for the next time
  {
    if (!(start_word = get_token(p, 0, &msg)))
      return msg;
    if (*start_word == '{') {
      p->words--;				// discard the brace
      p->ends_by_brace = 1;
      break;
    }
  }
  return NULL;
}

/* Parsing multiple files */

static char *
parse_fastbuf(struct cf_context *cc, const char *name_fb, struct fastbuf *fb, uns depth)
{
  struct cf_parser_state *p = cc->parser;
  if (!p)
    p = cc->parser = xmalloc_zero(sizeof(*p) + MAX_LINE);
  p->name_parse_fb = name_fb;
  p->parse_fb = fb;
  p->line_num = 0;
  p->line = p->line_buf;
  *p->line = 0;

  char *err;
  while (1)
  {
    err = split_command(p);
    if (err)
      goto error;
    if (!p->words)
      return NULL;
    char *name = p->copy_buf.ptr + p->word_buf.ptr[0];
    char *pars[p->words-1];
    for (uns i=1; i<p->words; i++)
      pars[i-1] = p->copy_buf.ptr + p->word_buf.ptr[i];
    if (!strcasecmp(name, "include"))
    {
      if (p->words != 2)
	err = "Expecting one filename";
      else if (depth > 8)
	err = "Too many nested files";
      else if (*p->line && *p->line != '#')	// because the contents of line_buf is not re-entrant and will be cleared
	err = "The include command must be the last one on a line";
      if (err)
	goto error;
      struct fastbuf *new_fb = bopen_try(pars[0], O_RDONLY, 1<<14);
      if (!new_fb) {
	err = cf_printf("Cannot open file %s: %m", pars[0]);
	goto error;
      }
      uns ll = p->line_num;
      err = parse_fastbuf(cc, stk_strdup(pars[0]), new_fb, depth+1);
      p->line_num = ll;
      bclose(new_fb);
      if (err)
	goto error;
      p->parse_fb = fb;
      continue;
    }
    enum cf_operation op;
    char *c = strchr(name, ':');
    if (!c)
      op = strcmp(name, "}") ? OP_SET : OP_CLOSE;
    else {
      *c++ = 0;
      switch (Clocase(*c)) {
	case 's': op = OP_SET; break;
	case 'c': op = Clocase(c[1]) == 'l' ? OP_CLEAR: OP_COPY; break;
	case 'a': switch (Clocase(c[1])) {
		    case 'p': op = OP_APPEND; break;
		    case 'f': op = OP_AFTER; break;
		    default: op = OP_ALL;
		  }; break;
	case 'p': op = OP_PREPEND; break;
	case 'r': op = (c[1] && Clocase(c[2]) == 'm') ? OP_REMOVE : OP_RESET; break;
	case 'e': op = OP_EDIT; break;
	case 'b': op = OP_BEFORE; break;
	default: op = OP_SET; break;
      }
      if (strcasecmp(c, cf_op_names[op])) {
	err = cf_printf("Unknown operation %s", c);
	goto error;
      }
    }
    if (p->ends_by_brace)
      op |= OP_OPEN;
    err = cf_interpret_line(cc, name, op, p->words-1, pars);
    if (err)
      goto error;
  }
error:
  if (name_fb)
    msg(L_ERROR, "File %s, line %d: %s", name_fb, p->line_num, err);
  else if (p->line_num == 1)
    msg(L_ERROR, "Manual setting of configuration: %s", err);
  else
    msg(L_ERROR, "Manual setting of configuration, line %d: %s", p->line_num, err);
  return "included from here";
}

static int
load_file(struct cf_context *cc, const char *file)
{
  cf_init_stack(cc);
  struct fastbuf *fb = bopen_try(file, O_RDONLY, 1<<14);
  if (!fb) {
    msg(L_ERROR, "Cannot open %s: %m", file);
    return 1;
  }
  char *err_msg = parse_fastbuf(cc, file, fb, 0);
  bclose(fb);
  return !!err_msg || cf_done_stack(cc);
}

static int
load_string(struct cf_context *cc, const char *string)
{
  cf_init_stack(cc);
  struct fastbuf fb;
  fbbuf_init_read(&fb, (byte *)string, strlen(string), 0);
  char *msg = parse_fastbuf(cc, NULL, &fb, 0);
  return !!msg || cf_done_stack(cc);
}

/* Safe loading and reloading */

struct conf_entry {	/* We remember a list of actions to apply upon reload */
  cnode n;
  enum {
    CE_FILE = 1,
    CE_STRING = 2,
  } type;
  char *arg;
};

static void
cf_remember_entry(struct cf_context *cc, uns type, const char *arg)
{
  if (!cc->need_journal)
    return;
  if (!cc->postpone_commit)
    return;
  struct conf_entry *ce = cf_malloc(sizeof(*ce));
  ce->type = type;
  ce->arg = cf_strdup(arg);
  clist_add_tail(&cc->conf_entries, &ce->n);
}

int
cf_reload(const char *file)
{
  struct cf_context *cc = cf_get_context();
  cf_journal_swap();
  struct cf_journal_item *oldj = cf_journal_new_transaction(1);
  uns ec = cc->everything_committed;
  cc->everything_committed = 0;

  clist old_entries;
  clist_move(&old_entries, &cc->conf_entries);
  cc->postpone_commit = 1;

  int err = 0;
  if (file)
    err = load_file(cc, file);
  else
    CLIST_FOR_EACH(struct conf_entry *, ce, old_entries) {
      if (ce->type == CE_FILE)
	err |= load_file(cc, ce->arg);
      else
	err |= load_string(cc, ce->arg);
      if (err)
	break;
      cf_remember_entry(cc, ce->type, ce->arg);
    }

  cc->postpone_commit = 0;
  if (!err)
    err |= cf_done_stack(cc);

  if (!err) {
    cf_journal_delete();
    cf_journal_commit_transaction(1, NULL);
  } else {
    cc->everything_committed = ec;
    cf_journal_rollback_transaction(1, oldj);
    cf_journal_swap();
    clist_move(&cc->conf_entries, &old_entries);
  }
  return err;
}

int
cf_load(const char *file)
{
  struct cf_context *cc = cf_get_context();
  struct cf_journal_item *oldj = cf_journal_new_transaction(1);
  int err = load_file(cc, file);
  if (!err) {
    cf_journal_commit_transaction(1, oldj);
    cf_remember_entry(cc, CE_FILE, file);
    cc->config_loaded = 1;
  } else
    cf_journal_rollback_transaction(1, oldj);
  return err;
}

int
cf_set(const char *string)
{
  struct cf_context *cc = cf_get_context();
  struct cf_journal_item *oldj = cf_journal_new_transaction(0);
  int err = load_string(cc, string);
  if (!err) {
    cf_journal_commit_transaction(0, oldj);
    cf_remember_entry(cc, CE_STRING, string);
  } else
    cf_journal_rollback_transaction(0, oldj);
  return err;
}
