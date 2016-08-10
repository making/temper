/*
 * Standalone temperature logger
 *
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "pcsensor.h"

/* Calibration adjustments */
/* See http://www.pitt-pladdy.com/blog/_20110824-191017_0100_TEMPer_under_Linux_perl_with_Cacti/ */
static float scale = 1.0287;
static float offset = -0.85;

int main(){
  int passes = 0;
  float tempc = 0.0000;
  int sock0;
  struct sockaddr_in addr;
  struct sockaddr_in client;
  int len;
  int sock;
  int n;
  int yes = 1;
  char buf[2048];

  sock0 = socket(AF_INET, SOCK_STREAM, 0);

  addr.sin_family = AF_INET;
  addr.sin_port = htons(12345);
  addr.sin_addr.s_addr = INADDR_ANY;

  setsockopt(sock0, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

  if (bind(sock0, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    perror("bind");
    return 1;
  }

  if (listen(sock0, 5) != 0) {
    perror("listen");
    return 1;
  }

  while (1) {
    len = sizeof(client);
    sock = accept(sock0, (struct sockaddr *)&client, &len);

    if (sock < 0) {
      perror("accept");
      break;
    }

    printf("accepted\n");

    while (1) {
      do {
	usb_dev_handle* lvr_winusb = pcsensor_open();
	if (!lvr_winusb) {
	  /* Open fails sometime, sleep and try again */
	  sleep(1);
	}
	else {
	  tempc = pcsensor_get_temperature(lvr_winusb);
	  printf("tmp=%f\n", tempc);
	  pcsensor_close(lvr_winusb);
	}
      } while ((tempc > -0.0001 && tempc < 0.0001));

      if (!(tempc > -0.0001 && tempc < 0.0001)) {
	/* Apply calibrations */
	tempc = (tempc * scale) + offset;

	struct tm *utc;
	time_t t;
	t = time(NULL);
	utc = gmtime(&t);
		
	char dt[80];
	strftime(dt, 80, "%Y-%m-%dT%H:%M:%SZ", utc);

	memset(buf, 0, sizeof(buf));
	snprintf(buf, sizeof(buf),"{\"datetime\":\"%s\", \"temper\":%f}\r\n", dt, tempc);
	fflush(stdout);

	n = send(sock, buf, (int)strlen(buf), 0);
	if (n < 0) {
	  perror("send");
	  break;
	}
      }
      sleep(3);
    }
    close(sock);
  }

  close(sock0);
  return 0;
}
