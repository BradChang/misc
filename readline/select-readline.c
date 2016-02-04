#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>

int fd[2];
int running=1;

/* called inside readline() whenever full line gathered */
void cb_linehandler(char *line) {

  if (line == NULL) { running=0; goto done; } // user EOF
  if (*line=='\0') goto done; // ignore user's empty line

  if (!strcmp(line,"quit")) running=0;
  else {
    fprintf(stderr,"received [%s]\n", line);
    add_history(line);
  }

 done:
  if (line) free(line);
}
 
int main(int argc, char *argv[]) {
  char prompt[100];
  int readline_fd;
  fd_set fds;
  pid_t pid;

  pipe(fd); // [0]=read end, [1]=write end
  pid = fork();
  if (pid < 0) goto done;
  if (pid == (pid_t)0) { /* child here */
    sleep(10);
    close(fd[1]); // close write end
    return 0;
  }
  /* parent here */
  close(fd[1]);

  snprintf(prompt,sizeof(prompt),"('quit' to exit)%% ");
  using_history();

  /**************************************************
   * handle keyboard interactive input.
   *************************************************/
  rl_callback_handler_install(prompt, cb_linehandler);
  readline_fd = fileno(rl_instream);
  while(running) {
    FD_ZERO(&fds);
    FD_SET(readline_fd, &fds);
    FD_SET(fd[0], &fds);

    if ( select(FD_SETSIZE,&fds,NULL,NULL,NULL) < 0) {
      fprintf(stderr,"select: %s\n", strerror(errno));
      goto done;
    }

    /* handle user input, but terminate on server io/close */
    if (FD_ISSET(readline_fd, &fds)) rl_callback_read_char();
    if (FD_ISSET(fd[0], &fds)) { 
      fprintf(stderr,"Connection closed.\n");
      running=0;
    }
  }

 done:
  rl_callback_handler_remove();
  close(fd[0]); // close read end of pipe
  return 0;
}

