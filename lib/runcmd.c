/*
 *	UCW Library -- Running of Commands
 *
 *	(c) 2004 Martin Mares <mj@ucw.cz>
 *
 *	This software may be freely distributed and used according to the terms
 *	of the GNU Lesser General Public License.
 */

#include "lib/lib.h"

#include <stdlib.h>
#include <string.h>
#include <alloca.h>
#include <unistd.h>
#include <sys/wait.h>

void NONRET
exec_command_v(byte *cmd, va_list args)
{
  va_list cargs = args;
  int cnt = 2;
  byte *arg;
  while (arg = va_arg(cargs, byte *))
    cnt++;
  char **argv = alloca(sizeof(byte *) * cnt);
  argv[0] = cmd;
  cnt = 1;
  cargs = args;
  while (arg = va_arg(cargs, byte *))
    argv[cnt++] = arg;
  argv[cnt] = NULL;
  execv(cmd, argv);
  byte echo[256];
  echo_command_v(echo, sizeof(echo), cmd, args);
  log(L_ERROR, "Cannot execute %s: %m", echo);
  exit(255);
}

int
run_command_v(byte *cmd, va_list args)
{
  pid_t p = fork();
  if (p < 0)
    {
      log(L_ERROR, "fork() failed: %m");
      return 0;
    }
  else if (!p)
    exec_command_v(cmd, args);
  else
    {
      int stat;
      byte msg[EXIT_STATUS_MSG_SIZE];
      p = waitpid(p, &stat, 0);
      if (p < 0)
	die("waitpid() failed: %m");
      if (format_exit_status(msg, stat))
	{
	  byte echo[256];
	  echo_command_v(echo, sizeof(echo), cmd, args);
	  log(L_ERROR, "`%s' failed: %s", echo, msg);
	  return 0;
	}
      return 1;
    }
}

void
echo_command_v(byte *buf, int size, byte *cmd, va_list args)
{
  byte *limit = buf + size - 4;
  byte *p = buf;
  byte *arg = cmd;
  va_list cargs = args;
  do
    {
      int l = strlen(arg);
      if (p != buf && p < limit)
	*p++ = ' ';
      if (p+l > limit)
	{
	  memcpy(p, arg, limit-p);
	  strcpy(limit, "...");
	  return;
	}
      memcpy(p, arg, l);
      p += l;
    }
  while (arg = va_arg(cargs, byte *));
  *p = 0;
}

int
run_command(byte *cmd, ...)
{
  va_list args;
  va_start(args, cmd);
  int e = run_command_v(cmd, args);
  va_end(args);
  return e;
}

void NONRET
exec_command(byte *cmd, ...)
{
  va_list args;
  va_start(args, cmd);
  exec_command_v(cmd, args);
}

void
echo_command(byte *buf, int len, byte *cmd, ...)
{
  va_list args;
  va_start(args, cmd);
  echo_command_v(buf, len, cmd, args);
  va_end(args);
}

#ifdef TEST

int main(void)
{
  byte msg[1024];
  echo_command(msg, sizeof(msg), "/bin/echo", "datel", "strakapoud", NULL);
  log(L_INFO, "Running <%s>", msg);
  run_command("/bin/echo", "datel", "strakapoud", NULL);
  return 0;
}

#endif
