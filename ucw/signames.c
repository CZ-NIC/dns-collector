/*
 *	A List of Signal Names
 *
 *	(c) 2012 Martin Mares <mj@ucw.cz>
 */

#include <ucw/lib.h>
#include <ucw/signames.h>

#include <stdlib.h>
#include <string.h>
#include <signal.h>

struct sig_name {
  const char name[11];
  byte number;
};

#define S(sig) { #sig, sig }

static const struct sig_name sig_names[] = {
#ifdef SIGABRT
	S(SIGABRT),
#endif
#ifdef SIGALRM
	S(SIGALRM),
#endif
#ifdef SIGBUS
	S(SIGBUS),
#endif
#ifdef SIGCHLD
	S(SIGCHLD),
#endif
#ifdef SIGCONT
	S(SIGCONT),
#endif
#ifdef SIGFPE
	S(SIGFPE),
#endif
#ifdef SIGHUP
	S(SIGHUP),
#endif
#ifdef SIGILL
	S(SIGILL),
#endif
#ifdef SIGINT
	S(SIGINT),
#endif
#ifdef SIGIO
	S(SIGIO),
#endif
#ifdef SIGIOT
	S(SIGIOT),
#endif
#ifdef SIGKILL
	S(SIGKILL),
#endif
#ifdef SIGPIPE
	S(SIGPIPE),
#endif
#ifdef SIGPOLL
	S(SIGPOLL),
#endif
#ifdef SIGPROF
	S(SIGPROF),
#endif
#ifdef SIGPWR
	S(SIGPWR),
#endif
#ifdef SIGQUIT
	S(SIGQUIT),
#endif
#ifdef SIGSEGV
	S(SIGSEGV),
#endif
#ifdef SIGSTKFLT
	S(SIGSTKFLT),
#endif
#ifdef SIGSTOP
	S(SIGSTOP),
#endif
#ifdef SIGSYS
	S(SIGSYS),
#endif
#ifdef SIGTERM
	S(SIGTERM),
#endif
#ifdef SIGTRAP
	S(SIGTRAP),
#endif
#ifdef SIGTSTP
	S(SIGTSTP),
#endif
#ifdef SIGTTIN
	S(SIGTTIN),
#endif
#ifdef SIGTTOU
	S(SIGTTOU),
#endif
#ifdef SIGURG
	S(SIGURG),
#endif
#ifdef SIGUSR1
	S(SIGUSR1),
#endif
#ifdef SIGUSR2
	S(SIGUSR2),
#endif
#ifdef SIGVTALRM
	S(SIGVTALRM),
#endif
#ifdef SIGWINCH
	S(SIGWINCH),
#endif
#ifdef SIGXCPU
	S(SIGXCPU),
#endif
#ifdef SIGXFSZ
	S(SIGXFSZ),
#endif
};

int
sig_name_to_number(const char *name)
{
  for (uns i=0; i < ARRAY_SIZE(sig_names); i++)
    if (!strcmp(sig_names[i].name, name))
      return sig_names[i].number;
  return -1;
}

const char *
sig_number_to_name(int number)
{
  for (uns i=0; i < ARRAY_SIZE(sig_names); i++)
    if (sig_names[i].number == number)
      return sig_names[i].name;
  return NULL;
}

#ifdef TEST

#include <stdio.h>

int main(void)
{
  char c[256];
  while (fgets(c, sizeof(c), stdin))
    {
      char *e = strchr(c, '\n');
      if (e)
	*e = 0;
      if (c[0] == '#')
	{
	  const char *name = sig_number_to_name(atoi(c+1));
	  puts(name ? : "?");
	}
      else
	{
	  int num = sig_name_to_number(c);
	  if (num < 0)
	    puts("?");
	  else
	    printf("%d\n", num);
	}
    }
  return 0;
}

#endif
