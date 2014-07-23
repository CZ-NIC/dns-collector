/*
 *	UCW Library -- Test of Tableprinter Types
 *
 *	(c) 2014 Robert Kessl <robert.kessl@economia.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include <ucw/lib.h>
#include <ucw/mempool.h>
#include <ucw/xtypes.h>
#include <ucw/table-types.h>

#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>

static void test_size_correct(struct fastbuf *out)
{
  static const char *size_strs[] = {
    "4",
    "4KB",
    "4MB",
    "4GB",
    "4TB",
    NULL
  };

  static u64 size_parsed[] = {
    4LLU,
    4 * 1024LLU,
    4 * 1024LLU * 1024LLU,
    4 * 1024LLU * 1024LLU * 1024LLU,
    4 * 1024LLU * 1024LLU * 1024LLU * 1024LLU
  };

  struct mempool *pool = mp_new(4096);

  uint i = 0;
  while(size_strs[i] != NULL) {
    u64 result;
    const char *parse_err = xt_size.parse(size_strs[i], &result, pool);

    ASSERT_MSG(parse_err == NULL, "Unexpected error in xt_size.parse");
    ASSERT_MSG(size_parsed[i] == result, "xt_size.parse parsed an incorrect value.");

    const char *result_str = xt_size.format(&result, i | SIZE_UNITS_FIXED, pool);
    bprintf(out, "%s %s\n", size_strs[i], result_str);

    i++;
  }

  mp_delete(pool);
}

static void test_size_parse_errors(struct fastbuf *out)
{
  static const char *size_strs[] = {
    "1X",
    "KB",
    "\0",
    NULL
  };

  uint i = 0;
  struct mempool *pool = mp_new(4096);

  while(size_strs[i] != NULL) {
    u64 result;
    const char *parse_err = xt_size.parse(size_strs[i], &result, pool);
    if(parse_err == NULL) {
      bprintf(out, "xt_size.parse did not result in error while parsing: '%s'.\n", size_strs[i]);
    } else {
      bprintf(out, "xt_size.parse error: '%s'.\n", parse_err);
    }

    i++;
  }

  mp_delete(pool);
}

static void test_bool_correct(struct fastbuf *out UNUSED)
{
  static const char *bool_strs[] = {
    "0",
    "1",
    "false",
    "true",
    NULL
  };

  static bool bool_parsed[] = {
    false,
    true,
    false,
    true
  };

  struct mempool *pool = mp_new(4096);
  uint i = 0;

  while(bool_strs[i] != NULL) {
    bool result;
    const char *err_str = xt_bool.parse(bool_strs[i], &result, pool);
    ASSERT_MSG(err_str == NULL, "Unexpected error in xt_bool.parse.");
    ASSERT_MSG(bool_parsed[i] == result, "xt_bool.parse parsed an incorrect value.");
    bprintf(out, "%s %s\n", bool_strs[i], result ? "true" : "false");
    i++;
  }

  mp_delete(pool);
}

int main(void)
{
  struct fastbuf *out;
  out = bfdopen_shared(1, 4096);

  test_size_correct(out);
  test_size_parse_errors(out);
  test_bool_correct(out);
  bclose(out);

  return 0;
}
