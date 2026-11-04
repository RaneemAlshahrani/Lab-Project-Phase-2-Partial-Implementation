/**
 * @file shell.h
 * @brief Command loop for a successfully initialized App.
 */

#ifndef MINIOS_SHELL_H
#define MINIOS_SHELL_H
#include "core.h"

/* Run commands, always clean up on return; 0=success, nonzero=runtime/I/O error. */
int shell_run(App *app, bool interactive);

#endif /* MINIOS_SHELL_H */
