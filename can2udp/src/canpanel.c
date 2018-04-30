/*
 * Copyright 2018 Joerg Dorchain
 *
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Joerg Dorchain wrote this file. As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.
 */


#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include "mcan.h"

/* Global stuff */
#define VERS_HIGH 0
#define VERS_LOW 1

#define _str(a) #a
#define str(a) _str(a)
#define VERSION str(VERS_HIGH) "." str(VERS_LOW)

#define BIT(i) (1<<i)

/* What we listen for */
#define UID 0x53383842		/* S88'42' */
#define SYSID 0x0042		/* default, from above */
#define DEVTYPE 0x0040		

#define DCC_BASE 0x3800

#define DEF_INT "can0"

#define PIDFILE "/var/run/canpanel.pid"

CanDevice cd = {
    .versHigh = VERS_HIGH,
    .versLow = VERS_LOW,
    .name = "CP-Can",
    .artNum = str(SYSID),
    .boardNum = 1,
    .hash = 0,
    .uid = UID,
    .type = DEVTYPE,
};

uint32_t bptol(uint8_t * p) {
  return *((uint32_t *) p);
}
uint16_t bptos(uint8_t * p) {
  return *((uint16_t *) p);
}

void usage(char *p)
{
  fprintf(stderr, "\nUsage %s [-v] [-f] [-b <int>] [-e <id>]\n", p);
  fprintf(stderr, "  Version: " VERSION "\n");
  fprintf(stderr, "  -v          verbose\n");
  fprintf(stderr, "  -f          foreground\n");
  fprintf(stderr, "  -b <int>    Connect to can intface\n");
  fprintf(stderr, "  -e <id>     set id\n");
}

int main(int argc, char **argv)
{
    struct sockaddr_can caddr;
    char *canint = DEF_INT;
    struct ifreq ifr;
    fd_set rfds;
    MCANMSG can_frame_in;
    uint16_t sysid = SYSID;
    int canfd;
    uint16_t addr, pos;
    int verbose = 0, foreground = 0;
    int opt;
    int pidfd;

    while ((opt = getopt(argc, argv, "vfb:e:h?")) != -1)
      switch (opt) {
      case 'v':
          verbose = 1;
	  break;
      case 'f':
          foreground = 1;
	  break;
      case 'b':
          canint = strdup(optarg);
	  break;
      case 'e':
          sysid = strtol(optarg, NULL, 0);
	  break;
      case 'h':
      case '?':
          usage(argv[0]);
	  exit(EXIT_SUCCESS);
      default:
          usage(argv[0]);
	  exit(EXIT_FAILURE);
      }

    cd.hash = generateHash(UID);

    canfd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (canfd < 0) {
	fprintf(stderr, "creating CAN socket error: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }
    memset(&caddr, 0, sizeof(caddr));
    caddr.can_family = AF_CAN;
    memset(ifr.ifr_name, 0, sizeof(ifr.ifr_name));
    strncpy(ifr.ifr_name, canint, sizeof(ifr.ifr_name));
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = 0;
    if (ioctl(canfd, SIOCGIFINDEX, &ifr) < 0) {
	fprintf(stderr, "setup CAN interfacce %s error: %s\n", ifr.ifr_name, strerror(errno));
	exit(EXIT_FAILURE);
    }
    caddr.can_ifindex = ifr.ifr_ifindex;
    if (bind(canfd, (struct sockaddr *)&caddr, sizeof(caddr)) < 0) {
	fprintf(stderr, "binding CAN socket error: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }
    if (!foreground) {
        if ((pidfd = open(PIDFILE,  O_RDWR|O_CREAT|O_EXCL|O_NOCTTY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0 ) {
            fprintf(stderr, "Cannot open pidfile %s: %s\n", PIDFILE, strerror(errno));
            exit(EXIT_FAILURE);
        }
        if (daemon(0, 0) < 0) {
            fprintf(stderr, "Cannot daemon()ise: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        /* TODO: Catch errors here */
        dprintf(pidfd, "%d\n", getpid());
        close(pidfd);
    }

/* main loop */
    while (1) {
    FD_ZERO(&rfds);
    FD_SET(canfd, &rfds);
    select(canfd + 1, &rfds, NULL, NULL, NULL);
    /* handle can */
    can_frame_in = getCanFrame(canfd);
    /* we are transparent - not much to listen to */
    /* Event from our device */
    if ((can_frame_in.cmd == S88_EVENT) && (can_frame_in.resp_bit == 1) &&
        (can_frame_in.hash == cd.hash) && (can_frame_in.dlc == 8) &&
	(sysid == ntohs(bptos(can_frame_in.data)))) {
      addr = ntohs(bptos(can_frame_in.data + 2)) / 2;
      pos = ntohs(bptos(can_frame_in.data + 2)) % 2;
printf("Sending acc frame addr %d pos %d power %d\n", addr, pos, can_frame_in.data[5]);
      sendAccessoryFrame(canfd, cd, DCC_BASE + addr, pos, 0, can_frame_in.data[5]);
    }
    /* id change for our device */
    if ((can_frame_in.cmd == SYS_CMD) && (can_frame_in.data[4] == SYS_ID) &&
    	(ntohl(bptol(can_frame_in.data)) == cd.uid) && (can_frame_in.resp_bit == 1)) {
	sysid = ntohs(bptos(can_frame_in.data + 5));
	if (verbose) 
	    fprintf(stderr, "Assigned new sysid 0x%04x\n", sysid);
      }
    }
    /* not reached */
}
