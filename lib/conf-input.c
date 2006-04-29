/*
 *	UCW Library -- Configuration files: parsing input streams
 *
 *	(c) 2001--2006 Robert Spalek <robert@ucw.cz>
 *	(c) 2003--2006 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"
#include "lib/conf.h"
#include "lib/getopt.h"
#include "lib/conf-internal.h"
#include "lib/mempool.h"
#include "lib/fastbuf.h"
#include "lib/chartype.h"
#include "lib/stkstring.h"

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

/* Text file parser */

static byte *name_parse_fb;
static struct fastbuf *parse_fb;
static uns line_num;

#define MAX_LINE	4096
static byte line_buf[MAX_LINE];
static byte *line = line_buf;

#include "lib/bbuf.h"
static bb_t copy_buf;
static uns copied;

#define GBUF_TYPE	uns
#define GBUF_PREFIX(x)	split_##x
#include "lib/gbuf.h"
static split_t word_buf;
static uns words;
static uns ends_by_brace;		// the line is ended by "{"

static int
get_line(void)
{
  if (!bgets(parse_fb, line_buf, MAX_LINE))
    return 0;
  line_num++;
  line = line_buf;
  while (Cblank(*line))
    line++;
  return 1;
}

static void
append(byte *start, byte *end)
{
  uns len = end - start;
  bb_grow(&copy_buf, copied + len + 1);
  memcpy(copy_buf.ptr + copied, start, len);
  copied += len + 1;
  copy_buf.ptr[copied-1] = 0;
}

#define	CONTROL_CHAR(x) (x == '{' || x == '}' || x == ';')
  // these characters separate words like blanks

static byte *
get_word(uns is_command_name)
{
  if (*line == '\'') {
    line++;
    while (1) {
      byte *start = line;
      while (*line && *line != '\'')
	line++;
      append(start, line);
      if (*line)
	break;
      copy_buf.ptr[copied-1] = '\n';
      if (!get_line())
	return "Unterminated apostrophe word at the end";
    }
    line++;

  } else if (*line == '"') {
    line++;
    uns start_copy = copied;
    while (1) {
      byte *start = line;
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
      append(start, line);
      if (*line)
	break;
      if (!escape)
	copy_buf.ptr[copied-1] = '\n';
      else // merge two lines
	copied -= 2;
      if (!get_line())
	return "Unterminated quoted word at the end";
    }
    line++;

    byte *tmp = stk_str_unesc(copy_buf.ptr + start_copy);
    uns l = strlen(tmp);
    bb_grow(&copy_buf, start_copy + l + 1);
    strcpy(copy_buf.ptr + start_copy, tmp);
    copied = start_copy + l + 1;

  } else {
    // promised that *line is non-null and non-blank
    byte *start = line;
    while (*line && !Cblank(*line) && !CONTROL_CHAR(*line)
	&& (*line != '=' || !is_command_name))
      line++;
    if (*line == '=') {				// nice for setting from a command-line
      if (line == start)
	return "Assignment without a variable";
      *line = ' ';
    }
    if (line == start)				// already the first char is control
      line++;
    append(start, line);
  }
  while (Cblank(*line))
    line++;
  return NULL;
}

static byte *
get_token(uns is_command_name, byte **msg)
{
  *msg = NULL;
  while (1) {
    if (!*line || *line == '#') {
      if (!is_command_name || !get_line())
	return NULL;
    } else if (*line == ';') {
      *msg = get_word(0);
      if (!is_command_name || *msg)
	return NULL;
    } else if (*line == '\\' && !line[1]) {
      if (!get_line()) {
	*msg = "Last line ends by a backslash";
	return NULL;
      }
      if (!*line || *line == '#')
	log(L_WARN, "The line %s:%d following a backslash is empty", name_parse_fb, line_num);
    } else {
      split_grow(&word_buf, words+1);
      uns start = copied;
      word_buf.ptr[words++] = copied;
      *msg = get_word(is_command_name);
      return *msg ? NULL : copy_buf.ptr + start;
    }
  }
}

static byte *
split_command(void)
{
  words = copied = ends_by_brace = 0;
  byte *msg, *start_word;
  if (!(start_word = get_token(1, &msg)))
    return msg;
  if (*start_word == '{')			// only one opening brace
    return "Unexpected opening brace";
  while (*line != '}')				// stays for the next time
  {
    if (!(start_word = get_token(0, &msg)))
      return msg;
    if (*start_word == '{') {
      words--;					// discard the brace
      ends_by_brace = 1;
      break;
    }
  }
  return NULL;
}

/* Parsing multiple files */

static byte *
parse_fastbuf(byte *name_fb, struct fastbuf *fb, uns depth)
{
  byte *msg;
  name_parse_fb = name_fb;
  parse_fb = fb;
  line_num = 0;
  line = line_buf;
  *line = 0;
  while (1)
  {
    msg = split_command();
    if (msg)
      goto error;
    if (!words)
      return NULL;
    byte *name = copy_buf.ptr + word_buf.ptr[0];
    byte *pars[words-1];
    for (uns i=1; i<words; i++)
      pars[i-1] = copy_buf.ptr + word_buf.ptr[i];
    if (!strcasecmp(name, "include"))
    {
      if (words != 2)
	msg = "Expecting one filename";
      else if (depth > 8)
	msg = "Too many nested files";
      else if (*line && *line != '#')		// because the contents of line_buf is not re-entrant and will be cleared
	msg = "The input command must be the last one on a line";
      if (msg)
	goto error;
      struct fastbuf *new_fb = bopen_try(pars[0], O_RDONLY, 1<<14);
      if (!new_fb) {
	msg = cf_printf("Cannot open file %s: %m", pars[0]);
	goto error;
      }
      uns ll = line_num;
      msg = parse_fastbuf(stk_strdup(pars[0]), new_fb, depth+1);
      line_num = ll;
      bclose(new_fb);
      if (msg)
	goto error;
      parse_fb = fb;
      continue;
    }
    enum cf_operation op;
    byte *c = strchr(name, ':');
    if (!c)
      op = strcmp(name, "}") ? OP_SET : OP_CLOSE;
    else {
      *c++ = 0;
      switch (Clocase(*c)) {
	case 's': op = OP_SET; break;
	case 'c': op = Clocase(c[1]) == 'l' ? OP_CLEAR: OP_COPY; break;
	case 'a': op = Clocase(c[1]) == 'p' ? OP_APPEND : OP_AFTER; break;
	case 'p': op = OP_PREPEND; break;
	case 'r': op = OP_REMOVE; break;
	case 'e': op = OP_EDIT; break;
	case 'b': op = OP_BEFORE; break;
	default: op = OP_SET; break;
      }
      if (strcasecmp(c, cf_op_names[op])) {
	msg = cf_printf("Unknown operation %s", c);
	goto error;
      }
    }
    if (ends_by_brace)
      op |= OP_OPEN;
    msg = cf_interpret_line(name, op, words-1, pars);
    if (msg)
      goto error;
  }
error:
  log(L_ERROR, "File %s, line %d: %s", name_fb, line_num, msg);
  return "included from here";
}

#ifndef DEFAULT_CONFIG
#define DEFAULT_CONFIG NULL
#endif
byte *cf_def_file = DEFAULT_CONFIG;

static uns postpone_commit;			// only for cf_getopt()
static uns everything_committed;		// after the 1st load, this flag is set on

static int
done_stack(void)
{
  if (cf_check_stack())
    return 1;
  if (cf_commit_all(postpone_commit ? CF_NO_COMMIT : everything_committed ? CF_COMMIT : CF_COMMIT_ALL))
    return 1;
  if (!postpone_commit)
    everything_committed = 1;
  return 0;
}

static int
load_file(byte *file)
{
  cf_init_stack();
  struct fastbuf *fb = bopen_try(file, O_RDONLY, 1<<14);
  if (!fb) {
    log(L_ERROR, "Cannot open %s: %m", file);
    return 1;
  }
  byte *msg = parse_fastbuf(file, fb, 0);
  bclose(fb);
  int err = !!msg || done_stack();
  if (!err)
    cf_def_file = NULL;
  return err;
}

static int
load_string(byte *string)
{
  cf_init_stack();
  struct fastbuf fb;
  fbbuf_init_read(&fb, string, strlen(string), 0);
  byte *msg = parse_fastbuf("memory string", &fb, 0);
  return !!msg || done_stack();
}

/* Safe loading and reloading */

int
cf_reload(byte *file)
{
  cf_journal_swap();
  struct cf_journal_item *oldj = cf_journal_new_transaction(1);
  uns ec = everything_committed;
  everything_committed = 0;
  int err = load_file(file);
  if (!err)
  {
    cf_journal_delete();
    cf_journal_commit_transaction(1, NULL);
  }
  else
  {
    everything_committed = ec;
    cf_journal_rollback_transaction(1, oldj);
    cf_journal_swap();
  }
  return err;
}

int
cf_load(byte *file)
{
  struct cf_journal_item *oldj = cf_journal_new_transaction(1);
  int err = load_file(file);
  if (!err)
    cf_journal_commit_transaction(1, oldj);
  else
    cf_journal_rollback_transaction(1, oldj);
  return err;
}

int
cf_set(byte *string)
{
  struct cf_journal_item *oldj = cf_journal_new_transaction(0);
  int err = load_string(string);
  if (!err)
    cf_journal_commit_transaction(0, oldj);
  else
    cf_journal_rollback_transaction(0, oldj);
  return err;
}

/* Command-line parser */

static void
load_default(void)
{
  if (cf_def_file)
    if (cf_load(cf_def_file))
      die("Cannot load default config %s", cf_def_file);
}

static void
final_commit(void)
{
  if (postpone_commit) {
    postpone_commit = 0;
    if (done_stack())
      die("Cannot commit after the initialization");
  }
}

int
cf_getopt(int argc, char * const argv[], const char *short_opts, const struct option *long_opts, int *long_index)
{
  static int other_options = 0;
  while (1) {
    int res = getopt_long (argc, argv, short_opts, long_opts, long_index);
    if (res == 'S' || res == 'C' || res == 0x64436667)
    {
      if (other_options)
	die("The -S and -C options must precede all other arguments");
      if (res == 'S') {
	postpone_commit = 1;
	load_default();
	if (cf_set(optarg))
	  die("Cannot set %s", optarg);
      } else if (res == 'C') {
	postpone_commit = 1;
	if (cf_load(optarg))
	  die("Cannot load config file %s", optarg);
      }
#ifdef CONFIG_DEBUG
      else {   /* --dumpconfig */
	load_default();
	final_commit();
	struct fastbuf *b = bfdopen(1, 4096);
	cf_dump_sections(b);
	bclose(b);
	exit(0);
      }
#endif
    } else {
      /* unhandled option or end of options */
      if (res != ':' && res != '?')
	load_default();
      final_commit();
      other_options++;
      return res;
    }
  }
}

