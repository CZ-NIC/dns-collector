/*
 *	Catching of signals and calling callback functions
 *
 *	(c) 2004, Robert Spalek <robert@ucw.cz>
 */

#include "lib/lib.h"

#include <stdlib.h>
#include <signal.h>

my_sighandler_t signal_handler[_NSIG];

static void
signal_handler_internal(int sig)
{
  signal(sig, signal_handler_internal);
  if (signal_handler[sig])
    signal_handler[sig]();
  abort();
}

void *
handle_signal(int signum)
{
  return signal(signum, signal_handler_internal);
}
