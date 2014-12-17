#ifndef CONTROL_PORT_H
#define CONTROL_PORT_H

#include <stdlib.h>

typedef int (cp_cmd_f)(void *cp, int argc, char **argv, void *data);

typedef struct {
  char *name;
  cp_cmd_f *cmdf;
  char *help;
  // return values from a control port command callback
#define CP_OK    ( 0)
#define CP_CLOSE (-1)
} cp_cmd_t;

/******************************************************************************
 * cp_init:     lib creates a unix domain socket, gives app listener fd to poll
 * cp_add_cmd:  app registers additional command
 *              in command callback, return CP_OK or CP_CLOSE to close client
 * cp_service:  app should call whenever listener fd or a client fd is readable
 *              returns fd for app to poll (if > 0)
 *              returns fd to stop polling (if < 0)
 * cp_fini:     closes the control port, terminates any clients
 * cp_printf:   used inside a command callback
 *****************************************************************************/
void *cp_init(char *path, cp_cmd_t *cmds, void *data, int *fd);
void  cp_add_cmd(void*, char *name, cp_cmd_f *cmd, char *help);
int   cp_service(void*, int fd);
void  cp_fini(void*);
void  cp_printf(void *, char *fmt, ...);

#endif 
