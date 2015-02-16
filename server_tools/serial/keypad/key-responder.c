#include <termios.h>
#include <stdlib.h>
#include <string.h> 
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include "tconf.h"

char *cmds[10];

tconf_t tc[] = {
                {"1", tconf_str, &cmds[1]},
                {"2", tconf_str, &cmds[2]},
                {"3", tconf_str, &cmds[3]},
                {"4", tconf_str, &cmds[4]},
                {"5", tconf_str, &cmds[5]},
                {"6", tconf_str, &cmds[6]},
                {"7", tconf_str, &cmds[7]},
                {"8", tconf_str, &cmds[8]},
                {"9", tconf_str, &cmds[9]},
               };


/* for further information on serial programming with termios 
 * see http://en.wikibooks.org/wiki/Serial_Programming/termios
   This code is based on an example on that site. */
 
void usage(char *prog) {
  fprintf(stderr, "usage: %s <cfgfile> <dev>\n",prog); 
  fprintf(stderr, "device is /dev/ttyACM0 or similar\n"); 
  fprintf(stderr, "tip: GNU screen can read serial devices too, screen <dev>\n"); 
  exit(-1);
}

int main(int argc,char** argv)
{
  int key;
  char *cmd;

  char *file =(argc > 1) ? argv[1] : "keys.conf";
  char *dev = (argc > 2) ? argv[2] : "/dev/ttyACM0";
  if (tconf(file, tc, sizeof(tc)/sizeof(*tc), 0)) exit(-1);

  struct termios tio;
  memset(&tio,0,sizeof(tio));
  tio.c_iflag=0;
  tio.c_oflag=0;
  tio.c_cflag=CS8|CREAD|CLOCAL; // 8n1, see termios.h 
  tio.c_lflag=0;
  tio.c_cc[VMIN]=1;
  tio.c_cc[VTIME]=5;
 
  int tty_fd;
  tty_fd=open(dev, O_RDWR);      
  if (tty_fd == -1) { 
    fprintf(stderr,"open %s: %s\n", dev, strerror(errno)); 
    exit(-1);
  }
  cfsetospeed(&tio,B9600);  // 9600 baud
  cfsetispeed(&tio,B9600);  // 9600 baud
  tcsetattr(tty_fd,TCSANOW,&tio);

  char c;
  while (read(tty_fd,&c,1)>0) {
    key = c-'0'; if ((key < 1) || (key > 9)) continue;
    cmd = cmds[key];
    fprintf(stderr,"[%d] -> [%s]\n", key, cmd ? cmd : "n/a");
    if (system(cmd) == -1) fprintf(stderr,"system: error\n");
  }
  close(tty_fd);
}
