/*
 *	UCW Library -- Catching of signals and calling callback functions
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>

sh_sighandler_t signal_handler[_NSIG];

static void
signal_handler_internal(int sig)
{
  if (signal_handler[sig])
  {
    if (!signal_handler[sig](sig))
      return;
  }
  abort();
}

void
handle_signal(int signum, struct sigaction *oldact)
{
#if 0
  struct sigaction act;
  bzero(&act, sizeof(act));
  act.sa_handler = signal_handler_internal;
  act.sa_flags = SA_NOMASK;
  if (sigaction(signum, &act, oldact) < 0)
    die("sigaction: %m");
#endif
}

void
unhandle_signal(int signum, struct sigaction *oldact)
{
#if 0
  if (sigaction(signum, oldact, NULL) < 0)
    die("sigaction: %m");
#endif
}
