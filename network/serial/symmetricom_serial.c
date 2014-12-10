#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>

/* set up RPi
* sudo dpkg-reconfigure tzdata  (set Eastern time zone)
* vi /etc/ntp.conf              (set NTP server)
* vi /etc/resolv.conf           (DNS server)
* vi /etc/network/interaces     (static IP)
*/


/*
* screen /dev/ttyUSB0
* defaults OK (9600,8n1)
* password is "public"
* here is a transcript of the serial session
*
        ND-4 version 1.5 rev 5
        2003 Symmetricom, Inc.


        Hello user at serial port

        Password: ******

        -----------------[ CONFIGURATION OPTIONS ]-----------------
        (1) Set device name            (A) Set NTP server parameters
        (2) Set IP configuration       (B) Set NTP broadcast options
        (3) Set/reset password         (C) Configure telnet interface
        (4) Set display intensity      (D) Set time zone bias
        (5) Set display type           (E) Set daylight savings time parameters
        (6) Set time                   (F) Set NTP client parameters
        (X) Exit session

        > 6
        Set UTC/GMT time (HH:MM:SS MM/DD/YYYY): 12:12:12 12/12/2012

*
* TRANSCRIPT ENDS
*
*/

/* for further information on serial programming with termios
* see http://en.wikibooks.org/wiki/Serial_Programming/termios
   This code is based on an example on that site. */

void usage(char *prog) {
  fprintf(stderr, "usage: %s <dev>\n",prog);
  fprintf(stderr, "device is /dev/ttyS0 or similar\n");
  fprintf(stderr, "tip: GNU screen can read serial devices too, screen <dev>\n");
  exit(-1);
}

void snd(int fd, char *x) {
int rc;
fprintf(stderr,"writing %s\n", x);
if (write(fd, x, strlen(x)) != strlen(x)) {
   fprintf(stderr,"write: %s\n", strerror(errno));
   exit(-1);
}
//sleep(5);
char buf[100];
while ( (rc = read(fd,buf,sizeof(buf))) >0) {
    fprintf(stderr,"response [%.*s]\n",rc,buf);
}
}

int main(int argc,char** argv)
{
  char *dev = (argc > 1) ? argv[1] : NULL;
  if (!dev) usage(argv[0]);
  time_t now = time(NULL);
  char time_str[200];

  /* format the time like this "12:12:12 12/12/2012\r\n */
  struct tm *tm = localtime(&now);

  snprintf(time_str,sizeof(time_str),"%.2u:%.2u:%.2u %.2u/%.2u/%u",
    tm->tm_hour,
    tm->tm_min,
    tm->tm_sec,
    tm->tm_mon+1,
    tm->tm_mday,
    1900+tm->tm_year);

  fprintf(stderr,"time is %s\n", time_str);
  strcat(time_str,"\r\n");
  //exit(-1);

  struct termios tio;
  memset(&tio,0,sizeof(tio));
  tio.c_iflag=0;
  tio.c_oflag=0;
  tio.c_cflag=CS8|CREAD|CLOCAL; // 8n1, see termios.h
  tio.c_lflag=0;
  tio.c_cc[VMIN]=1;
  tio.c_cc[VTIME]=5;

  int tty_fd;
  tty_fd=open(argv[1], O_RDWR);
  cfsetospeed(&tio,B9600);  // 9600 baud
  cfsetispeed(&tio,B9600);  // 9600 baud
  tcsetattr(tty_fd,TCSANOW,&tio);

  int fl;
  fl = fcntl(tty_fd, F_GETFL);
  fcntl(tty_fd, F_SETFL, fl | O_NONBLOCK);

  char *b;
  b = "\r\n\r\n\r\n";            snd(tty_fd,b);  // wake up
  b = "public\r\n";              snd(tty_fd,b);  // password
  b = "6\r\n";                   snd(tty_fd,b);  // menu option 6 (set time)
  b = time_str;                  snd(tty_fd,b);  // time and date
  // sleep(50); // takes 48 seconds for clock to handle last
  b = "X\r\n";                   snd(tty_fd,b);  // exit session

  fprintf(stderr, "done\n");
  close(tty_fd);
  return 0;
}

