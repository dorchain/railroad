/* ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <info@gerhard-bertelsmann.de> wrote this file. As long as you retain this
 * notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return
 * Gerhard Bertelsmann
 * ----------------------------------------------------------------------------
 */

/* Thanks to Stefan Krauss and the SocketCAN team
 */

#include "can2lan.h"

static char *CAN_FORMAT_STRG      = "      CAN->  CANID 0x%08X R [%d]";
static char *UDP_FORMAT_STRG      = "->CAN>UDP    CANID 0x%08X   [%d]";
static char *TCP_FORMAT_STRG      = "->TCP>CAN    CANID 0x%08X   [%d]";
static char *TCP_FORMATS_STRG     = "->TCP>CAN*   CANID 0x%08X   [%d]";
static char *CAN_TCP_FORMAT_STRG  = "->CAN>TCP    CANID 0x%08X   [%d]";
static char *NET_UDP_FORMAT_STRG  = "      UDP->  CANID 0x%08X   [%d]";
static char *NET_TCP_FORMAT_STRG  = "      TCP->  CANID 0x%08X   [%d]";

static unsigned char M_GLEISBOX_MAGIC_START_SEQUENCE[] = { 0x00, 0x36, 0x03, 0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00 };
static unsigned char M_CAN_PING[]                      = { 0x00, 0x30, 0x47, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
static unsigned char M_PING_RESPONSE[] = { 0x00, 0x30, 0x00, 0x00, 0x00 };

unsigned char GETCONFIG[]          = { 0x00, 0x40, 0xaf, 0x7e, 0x08 };
unsigned char GETCONFIG_DATA[]     = { 0x00, 0x42, 0x03, 0x00, 0x08 };

char *cs2_configs[][2] = {
    {"loks", "lokomotive.cs2"},
    {"mags", "magnetartikel.cs2"},
    {"fs", "fahrstrassen.cs2"},
    {"gbs", "gleisbild.cs2"},
/*    {NULL, NULL}, */
    {"lokstat", "lokomotive.sr2"},
    {"magstat", "magnetartikel.sr2"},
    {"gbsstat", "gbsstat.sr2"},
    {"fsstat", "fahrstrassen.sr2"},
    {NULL, NULL},
};

char config_dir[MAXLINE];
char config_file[MAXLINE];
char **page_name;
char **cs2_page_name;
int ms1_workaround;
struct timeval last_sent;

void print_usage(char *prg) {
    fprintf(stderr, "\nUsage: %s -c <config_dir> -u <udp_port> -t <tcp_port> -d <udp_dest_port> -i <can interface>\n", prg);
    fprintf(stderr, "   Version 1.11\n\n");
    fprintf(stderr, "         -c <config_dir>     set the config directory\n");
    fprintf(stderr, "         -u <port>           listening UDP port for the server - default 15731\n");
    fprintf(stderr, "         -t <port>           listening TCP port for the server - default 15731\n");
    fprintf(stderr, "         -s <port>           second listening TCP server port - default 15732\n");
    fprintf(stderr, "         -d <port>           destination UDP port for the server - default 15730\n");
    fprintf(stderr, "         -b <bcast_addr/int> broadcast address or interface - default 255.255.255.255/br-lan\n");
    fprintf(stderr, "         -i <can int>        CAN interface - default can0\n");
    fprintf(stderr, "         -k                  use a connected CS2.exe as config source\n");
    fprintf(stderr, "         -T                  timeout starting %s in sec - default %d sec\n", prg, MAX_UDP_BCAST_RETRY);
    fprintf(stderr, "         -m                  doing MS1 workaround - default: don't do it\n");
    fprintf(stderr, "         -f                  running in foreground\n");
    fprintf(stderr, "         -v                  verbose output (in foreground)\n\n");
}

int send_magic_start_60113_frame(int can_socket, int verbose) {
    if (frame_to_can(can_socket, M_GLEISBOX_MAGIC_START_SEQUENCE) < 0) {
	fprintf(stderr, "can't send CAN magic 60113 start sequence\n");
	return -1;
    } else {
	if (verbose) {
	    printf("                CAN magic 60113 start written\n");
	    print_can_frame(CAN_FORMAT_STRG, M_GLEISBOX_MAGIC_START_SEQUENCE, verbose);
	}
    }
    return 0;
}

int send_can_ping(int can_socket, int verbose) {
    if (frame_to_can(can_socket, M_CAN_PING) < 0) {
	fprintf(stderr, "can't send CAN Ping\n");
	return -1;
    } else {
	if (verbose) {
	    /* printf("                CAN Ping sent\n"); */
	    print_can_frame(CAN_FORMAT_STRG, M_CAN_PING, verbose);
	}
    }
    return 0;
}

int copy_cs2_config(struct cs2_config_data_t *cs2_config_data) {
    unsigned char newframe[CAN_ENCAP_SIZE];

    if (cs2_config_data->cs2_tcp_socket) {
	memset(newframe, 0, CAN_ENCAP_SIZE);
	memcpy(newframe, GETCONFIG, 5);
	/* TODO */

	if (cs2_config_data->verbose)
	    printf("getting %s filename %s\n", cs2_configs[0][0], cs2_configs[0][1]);
	cs2_config_data->name = cs2_configs[0][1];
	memcpy(&newframe[5], cs2_configs[0][0], strlen(cs2_configs[0][0]));
	cs2_config_data->next = 1;

	if (cs2_config_data->verbose)
	    printf("send to CS2.exe ...\n");
	net_to_net(cs2_config_data->cs2_tcp_socket, NULL, newframe, CAN_ENCAP_SIZE);
	/* done - don't copy again */
	cs2_config_data->cs2_config_copy = 0;
	cs2_config_data->state = CS2_STATE_NORMAL_CONFIG;
    } else {
	fprintf(stderr, "can't clone CS2 config - no CS2 TCP connection yet\n");
    }
    return 0;
}

int check_data_udp(int udp_socket, struct sockaddr *baddr, struct cs2_config_data_t *cs2_config_data, unsigned char *netframe) {
    uint32_t canid;

    memcpy(&canid, netframe, 4);
    canid = ntohl(canid);
    switch (canid & 0xFFFF0000UL) {
    case (0x00310000UL):
	if ((netframe[11] == 0xEE) && (netframe[12] == 0xEE)) {
	    if (cs2_config_data->verbose)
		printf("                received CAN ping\n");
	    memcpy(netframe, M_PING_RESPONSE, 5);
	    if (net_to_net(udp_socket, baddr, netframe, CAN_ENCAP_SIZE)) {
		fprintf(stderr, "sending UDP data (CAN Ping) error:%s \n", strerror(errno));
	    } else {
		print_can_frame(NET_UDP_FORMAT_STRG, netframe, cs2_config_data->verbose);
		if (cs2_config_data->verbose)
		    printf("                replied CAN ping\n");
	    }
	    if (cs2_config_data->cs2_config_copy)
		copy_cs2_config(cs2_config_data);
	}
	break;
    case (0x00420000UL):
	/* check for initiated config request */
	if (canid == 0x0042af7e) {
	    if (cs2_config_data->verbose)
		printf("copy config request\n");
	    cs2_config_data->cs2_config_copy = 1;
	    copy_cs2_config(cs2_config_data);
	}
	break;
    default:
	break;
    }
    return 0;
}

int check_data(int tcp_socket, struct cs2_config_data_t *cs2_config_data, unsigned char *netframe) {
    uint32_t canid;
    char config_name[9];
    char gbs_name[MAXLINE];
    int ret;
    gbs_name[0] = '\0';

    ret = 0;
    memcpy(&canid, netframe, 4);
    canid = ntohl(canid);
    switch (canid & 0xFFFF0000UL) {
    case (0x00310000UL):	/* CAN ping */
	ret = 1;
	/* looking for CS2.exe ping answer */
	print_can_frame(NET_TCP_FORMAT_STRG, netframe, cs2_config_data->verbose);
	if ((netframe[11] == 0xFF) && (netframe[12] == 0xFF)) {
	    if (cs2_config_data->verbose)
		printf("got CS2 TCP ping - copy config var: %d\n", cs2_config_data->cs2_config_copy);
	    cs2_config_data->cs2_tcp_socket = tcp_socket;
	}
	break;
    case (0x00400000UL):	/* config data */
	/* mark frame to send over CAN */
	ret = 0;
	/* check for special copy request */
	if (canid == 0x0040af7e) {
	    ret = 1;
	    netframe[1] |= 1;
	    netframe[4] = 4;
	    strcpy((char *)&netframe[5], "copy");
	    net_to_net(tcp_socket, NULL, netframe, CAN_ENCAP_SIZE);
	    if (cs2_config_data->verbose)
		printf("CS2 copy request\n");
	    cs2_config_data->cs2_config_copy = 1;
	} else {
	    strncpy(config_name, (char *)&netframe[5], 8);
	    config_name[8] = '\0';
	    if (cs2_config_data->verbose)
	        printf("%s ID 0x%08x %s\n", __func__, canid, (char *)&netframe[5]);
	    netframe[1] |= 1;
	    net_to_net(tcp_socket, NULL, netframe, CAN_ENCAP_SIZE);
	    if (strcmp("loks", config_name) == 0) {
		ret = 1;
		send_tcp_config_data("lokomotive.cs2", config_dir, canid, tcp_socket, CRC | COMPRESSED);
		break;
	    } else if (strcmp("mags", config_name) == 0) {
		ret = 1;
		send_tcp_config_data("magnetartikel.cs2", config_dir, canid, tcp_socket, CRC | COMPRESSED);
		break;
	    } else if (strncmp("gbs-", config_name, 4) == 0) {
		int page_number;
		ret = 1;
		page_number = atoi(&config_name[4]);
		strcat(gbs_name, "gleisbilder/");
		if (page_name) {
		    strcat(gbs_name, page_name[page_number]);
		    /* strcat(gbs_name, ".cs2"); */
		    send_tcp_config_data(gbs_name, config_dir, canid, tcp_socket, CRC | COMPRESSED);
		}
		break;
	    } else if (strcmp("gbs", config_name) == 0) {
		ret = 1;
		send_tcp_config_data("gleisbild.cs2", config_dir, canid, tcp_socket, CRC | COMPRESSED);
		break;
	    } else if (strcmp("fs", config_name) == 0) {
		ret = 1;
		send_tcp_config_data("fahrstrassen.cs2", config_dir, canid, tcp_socket, CRC | COMPRESSED);
		break;
	    }
	    /* TODO : these files depends on different internal states */
	    else if (strcmp("lokstat", config_name) == 0) {
		ret = 1;
		/* fprintf(stderr, "%s: lokstat (lokomotive.sr2) not implemented yet\n", __func__); */
		send_tcp_config_data("lokomotive.sr2", config_dir, canid, tcp_socket, CRC | COMPRESSED);
		break;
	    } else if (strcmp("magstat", config_name) == 0) {
		ret = 1;
		/* fprintf(stderr, "%s: magstat (magnetartikel.sr2) not implemented yet\n\n", __func__); */
		send_tcp_config_data("magnetartikel.sr2", config_dir, canid, tcp_socket, CRC | COMPRESSED);
		break;
	    } else if (strcmp("gbsstat", config_name) == 0) {
		ret = 1;
		/* fprintf(stderr, "%s: gbsstat (gbsstat.sr2) not implemented yet\n\n", __func__); */
		send_tcp_config_data("gbsstat.sr2", config_dir, canid, tcp_socket, CRC | COMPRESSED);
		break;
	    } else if (strcmp("fsstat", config_name) == 0) {
		ret = 1;
		/* fprintf(stderr, "%s: fsstat (fahrstrassen.sr2) not implemented yet\n\n", __func__); */
		send_tcp_config_data("fahrstrassen.sr2", config_dir, canid, tcp_socket, CRC | COMPRESSED);
		break;
	    }
	    break;
	}
    case (0x00420000UL):
	/* mark frame to not send over CAN */
	ret = 1;
	/* check for initiated copy request */
	reassemble_data(cs2_config_data, netframe);
	print_can_frame(NET_TCP_FORMAT_STRG, netframe, cs2_config_data->verbose);
	break;
	/* fake cyclic MS1 slave monitoring response */
    case (0x0C000000UL):
	/* mark CAN frame to send */
	ret = 0;
	if (ms1_workaround)
	    netframe[5] = 0x03;
	break;
    }
    return ret;
}

int main(int argc, char **argv) {
    pid_t pid;
    int n, i, max_fds, opt, max_tcp_i, nready, conn_fd, timeout, tcp_client[MAX_TCP_CONN];
    struct can_frame frame;
    char timestamp[16];
    /* UDP incoming socket , CAN socket, UDP broadcast socket, TCP socket */
    int sa, sc, sb, st, st2, tcp_socket;
    struct sockaddr_in saddr, baddr, tcp_addr, tcp_addr2;
    /* vars for determing broadcast address */
    struct ifaddrs *ifap, *ifa;
    struct sockaddr_in *bsa;
    struct sockaddr_can caddr;
    struct ifreq ifr;
    socklen_t caddrlen = sizeof(caddr);
    socklen_t tcp_client_length = sizeof(tcp_addr);
    fd_set all_fds, read_fds;
    int s, ret;
    struct timeval tv;
    char *udp_dst_address;
    char *bcast_interface;
    struct cs2_config_data_t cs2_config_data;

    int local_udp_port = 15731;
    int local_tcp_port = 15731;
    int local2_tcp_port = 15732;
    int destination_port = 15730;
    int background = 1;
    const int on = 1;
    char buffer[64];

    /* clear timestamp for last CAN frame sent */
    memset(&last_sent, 0, sizeof(last_sent));

    memset(&cs2_config_data, 0, sizeof(cs2_config_data));

    page_name = calloc(MAX_TRACK_PAGE, sizeof(char *));
    if (!page_name) {
	fprintf(stderr, "can't alloc memory for page_name: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    };

    cs2_page_name = calloc(MAX_TRACK_PAGE, sizeof(char *));
    if (!cs2_page_name) {
	fprintf(stderr, "can't alloc memory for cs2_page_name: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    };

    ms1_workaround = 0;

    cs2_config_data.verbose = 0;
    cs2_config_data.state = CS2_STATE_INACTIVE;
    cs2_config_data.page_name = cs2_page_name;
    cs2_config_data.cs2_config_copy = 0;
    cs2_config_data.cs2_tcp_socket = 0;
    cs2_config_data.track_index = MAX_TRACK_PAGE;

    memset(ifr.ifr_name, 0, sizeof(ifr.ifr_name));
    strcpy(ifr.ifr_name, "can0");
    memset(config_dir, 0, sizeof(config_dir));

    udp_dst_address = (char *)calloc(MAXIPLEN, 1);
    if (!udp_dst_address) {
	fprintf(stderr, "can't alloc memory for udp_dst_address: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    };

    bcast_interface = (char *)calloc(MAXIPLEN, 1);
    if (!bcast_interface) {
	fprintf(stderr, "can't alloc memory for bcast_interface: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    };

    timeout = MAX_UDP_BCAST_RETRY;
    strcpy(udp_dst_address, "255.255.255.255");
    strcpy(bcast_interface, "br-lan");

    config_file[0] = '\0';

    while ((opt = getopt(argc, argv, "c:u:s:t:d:b:i:kT:mvhf?")) != -1) {
	switch (opt) {
	case 'c':
	    if (strnlen(optarg, MAXLINE) < MAXLINE) {
		strncpy(config_dir, optarg, sizeof(config_dir) - 1);
	    } else {
		fprintf(stderr, "config file dir to long\n");
		exit(EXIT_FAILURE);
	    }
	    break;
	case 'u':
	    local_udp_port = strtoul(optarg, (char **)NULL, 10);
	    break;
	case 't':
	    local_tcp_port = strtoul(optarg, (char **)NULL, 10);
	    break;
	case 's':
	    local2_tcp_port = strtoul(optarg, (char **)NULL, 10);
	    break;
	case 'd':
	    destination_port = strtoul(optarg, (char **)NULL, 10);
	    break;
	case 'b':
	    if (strnlen(optarg, MAXIPLEN) <= MAXIPLEN - 1) {
		/* IP address begins with a number */
		if ((optarg[0] >= '0') && (optarg[0] <= '9')) {
		    memset(udp_dst_address, 0, MAXIPLEN);
		    strncpy(udp_dst_address, optarg, MAXIPLEN - 1);
		} else {
		    memset(udp_dst_address, 0, MAXIPLEN);
		    memset(bcast_interface, 0, MAXIPLEN);
		    strncpy(bcast_interface, optarg, MAXIPLEN - 1);
		}
	    } else {
		fprintf(stderr, "UDP broadcast address or interface error: %s\n", optarg);
		exit(EXIT_FAILURE);
	    }
	    break;
	case 'i':
	    strncpy(ifr.ifr_name, optarg, sizeof(ifr.ifr_name) - 1);
	    break;
	case 'k':
	    cs2_config_data.cs2_config_copy = 1;
	    break;
	case 'T':
	    timeout = strtoul(optarg, (char **)NULL, 10);
	    break;
	case 'm':
	    ms1_workaround = 1;
	    break;
	case 'v':
	    cs2_config_data.verbose = 1;
	    break;
	case 'f':
	    background = 0;
	    break;
	case 'h':
	case '?':
	    print_usage(basename(argv[0]));
	    exit(EXIT_SUCCESS);
	default:
	    fprintf(stderr, "Unknown option %c\n", opt);
	    print_usage(basename(argv[0]));
	    exit(EXIT_FAILURE);
	}
    }

    /* read track file */
    if (config_dir[0] == 0) {
	strcat(config_file, ".");
    }

    strcat(config_file, config_dir);
    if (config_dir[strlen(config_dir) - 1] != '/') {
	strcat(config_file, "/");
    }
    strncpy(config_dir, config_file, sizeof(config_dir) - 1);
    strcat(config_file, "gleisbild.cs2");

    cs2_config_data.dir = config_dir;

    page_name = read_track_file(config_file, page_name);

    /* we are trying to setup a UDP socket */
    for (i = 0; i < timeout; i++) {
	/* trying to get the broadcast address */
	getifaddrs(&ifap);
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
	    if (ifa->ifa_addr) {
		if (ifa->ifa_addr->sa_family == AF_INET) {
		    bsa = (struct sockaddr_in *)ifa->ifa_broadaddr;
		    if (strncmp(ifa->ifa_name, bcast_interface, strlen(bcast_interface)) == 0)
			udp_dst_address = inet_ntoa(bsa->sin_addr);
		}
	    }
	}
	/* try to prepare UDP sending socket struct */
	memset(&baddr, 0, sizeof(baddr));
	baddr.sin_family = AF_INET;
	baddr.sin_port = htons(destination_port);
	s = inet_pton(AF_INET, udp_dst_address, &baddr.sin_addr);
	if (s > 0)
	    break;
	sleep(1);
    }
    /* check if we got a real UDP socket after MAX_UDP_BCAST_TRY seconds */
    if (s <= 0) {
	if (s == 0) {
	    fprintf(stderr, "UDP IP address invalid\n");
	} else {
	    fprintf(stderr, "invalid address family\n");
	}
	exit(EXIT_FAILURE);
    }

    if (cs2_config_data.verbose & !background)
	printf("using broadcast address %s\n", udp_dst_address);

    /* prepare UDP sending socket */
    sb = socket(AF_INET, SOCK_DGRAM, 0);
    if (sb < 0) {
	fprintf(stderr, "error creating UDP sending socket: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }
    if (setsockopt(sb, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0) {
	fprintf(stderr, "error setup UDP broadcast option: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }

    /* prepare reading UDP socket */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(local_udp_port);
    sa = socket(PF_INET, SOCK_DGRAM, 0);
    if (sa < 0) {
	fprintf(stderr, "creating UDP reading socket error: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }
    if (bind(sa, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
	fprintf(stderr, "binding UDP reading socket error: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }

    /* prepare TCP socket */
    st = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (st < 0) {
	fprintf(stderr, "creating TCP socket error: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }
    /* disable Nagle */
    if (setsockopt(st, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0) {
	fprintf(stderr, "error disabling Nagle: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }

    tcp_addr.sin_family = AF_INET;
    tcp_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    tcp_addr.sin_port = htons(local_tcp_port);
    if (bind(st, (struct sockaddr *)&tcp_addr, sizeof(tcp_addr)) < 0) {
	fprintf(stderr, "binding TCP socket error: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }
    if (listen(st, MAXPENDING) < 0) {
	fprintf(stderr, "starting TCP listener error: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }
    /* prepare TCP clients array */
    max_tcp_i = -1;		/* index into tcp_client[] array */
    for (i = 0; i < MAX_TCP_CONN; i++)
	tcp_client[i] = -1;	/* -1 indicates available entry */

    /* prepare second TCP socket */
    st2 = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (st2 < 0) {
	fprintf(stderr, "creating second TCP socket error: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }
    tcp_addr2.sin_family = AF_INET;
    tcp_addr2.sin_addr.s_addr = htonl(INADDR_ANY);
    tcp_addr2.sin_port = htons(local2_tcp_port);
    if (bind(st2, (struct sockaddr *)&tcp_addr2, sizeof(tcp_addr2)) < 0) {
	fprintf(stderr, "binding second TCP socket error: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }
    if (listen(st2, MAXPENDING) < 0) {
	fprintf(stderr, "starting TCP listener error: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }
#if 0
    /* prepare TCP clients array */
    max_tcp_i = -1;		/* index into tcp_client[] array */
    for (i = 0; i < MAX_TCP_CONN; i++)
	tcp_client[i] = -1;	/* -1 indicates available entry */
#endif

    /* prepare CAN socket */
    memset(&caddr, 0, sizeof(caddr));
    sc = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sc < 0) {
	fprintf(stderr, "creating CAN socket error: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }
    caddr.can_family = AF_CAN;
    if (ioctl(sc, SIOCGIFINDEX, &ifr) < 0) {
	fprintf(stderr, "setup CAN error: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }
    caddr.can_ifindex = ifr.ifr_ifindex;

    if (bind(sc, (struct sockaddr *)&caddr, caddrlen) < 0) {
	fprintf(stderr, "binding CAN socket error: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }

    /* start Maerklin 60113 box */
    if (send_magic_start_60113_frame(sc, cs2_config_data.verbose))
	exit(EXIT_FAILURE);

    /* daemonize the process if requested */
    if (background) {
	/* fork off the parent process */
	pid = fork();
	if (pid < 0)
	    exit(EXIT_FAILURE);
	/* if we got a good PID, then we can exit the parent process */
	if (pid > 0) {
	    printf("Going into background ...\n");
	    exit(EXIT_SUCCESS);
	}
    }

    /* set select timeout -> send periodic CAN Ping */
    memset(&tv, 0, sizeof(tv));
    tv.tv_sec = 1;
    tv.tv_usec = 0;

    FD_ZERO(&all_fds);
    FD_SET(sc, &all_fds);
    FD_SET(sa, &all_fds);
    FD_SET(st, &all_fds);
    FD_SET(st2, &all_fds);
    max_fds = MAX(MAX(MAX(sc, sa), st), st2);

    while (1) {
	read_fds = all_fds;
	nready = select(max_fds + 1, &read_fds, NULL, NULL, &tv);
	if (nready == 0) {
	    /*    send_can_ping(sc); */
	    tv.tv_sec = 1;
	    tv.tv_usec = 0;
	    continue;
	} else if (nready < 0)
	    fprintf(stderr, "select error: %s\n", strerror(errno));

	tv.tv_sec = 1;
	tv.tv_usec = 0;

	/* received a CAN frame */
	if (FD_ISSET(sc, &read_fds)) {
	    /* reading via SockatCAN */
	    if (read(sc, &frame, sizeof(struct can_frame)) < 0) {
		fprintf(stderr, "reading CAN frame error: %s\n", strerror(errno));
	    }
	    /* if CAN Frame is EFF do it */
	    if (frame.can_id & CAN_EFF_FLAG) {	/* only EFF frames are valid */
		/* send UDP frame */
		frame_to_net(sb, (struct sockaddr *)&baddr, (struct can_frame *)&frame);
		print_can_frame(UDP_FORMAT_STRG, netframe, cs2_config_data.verbose & !background);
		/* send CAN frame to all connected TCP clients */
		/* TODO: need all clients the packets ? */
		for (i = 0; i <= max_tcp_i; i++) {	/* check all clients for data */
		    tcp_socket = tcp_client[i];
		    if (tcp_socket < 0)
			continue;
		    frame_to_net(tcp_socket, (struct sockaddr *)&tcp_addr, (struct can_frame *)&frame);
		    print_can_frame(CAN_TCP_FORMAT_STRG, netframe, cs2_config_data.verbose & !background);
		}
	    }
	}
	/* received a UDP packet */
	if (FD_ISSET(sa, &read_fds)) {
	    if (read(sa, netframe, MAXDG) == CAN_ENCAP_SIZE) {
		/* send packet on CAN */
		ret = frame_to_can(sc, netframe);
		print_can_frame(NET_UDP_FORMAT_STRG, netframe, cs2_config_data.verbose & !background);
		check_data_udp(sb, (struct sockaddr *)&baddr, &cs2_config_data, netframe);
	    }
	}
	/* received a TCP packet */
	if (FD_ISSET(st, &read_fds)) {
	    conn_fd = accept(st, (struct sockaddr *)&tcp_addr, &tcp_client_length);
	    if (cs2_config_data.verbose && !background) {
		printf("new client: %s, port %d conn fd: %d max fds: %d\n", inet_ntop(AF_INET, &(tcp_addr.sin_addr),
			buffer, sizeof(buffer)), ntohs(tcp_addr.sin_port), conn_fd, max_fds);
	    }
	    for (i = 0; i < MAX_TCP_CONN; i++) {
		if (tcp_client[i] < 0) {
		    tcp_client[i] = conn_fd;	/* save new TCP client descriptor */
		    break;
		}
	    }
	    if (i == MAX_TCP_CONN)
		fprintf(stderr, "too many TCP clients\n");

	    FD_SET(conn_fd, &all_fds);		/* add new descriptor to set */
	    max_fds = MAX(conn_fd, max_fds);	/* for select */
	    max_tcp_i = MAX(i, max_tcp_i);	/* max index in tcp_client[] array */
	    /* send embedded CAN ping */
	    memcpy(netframe, M_CAN_PING, CAN_ENCAP_SIZE);
	    net_to_net(conn_fd, NULL, netframe, CAN_ENCAP_SIZE);
	    if (cs2_config_data.verbose && !background)
		printf("send embedded CAN ping\n");

	    if (--nready <= 0)
		continue;			/* no more readable descriptors */
	}
	/* received a packet on second TCP port */
	if (FD_ISSET(st2, &read_fds)) {
	    conn_fd = accept(st2, (struct sockaddr *)&tcp_addr2, &tcp_client_length);

	    /* TODO : close missing */
	    if (cs2_config_data.verbose && !background) {
		printf("new client: %s, port %d conn fd: %d max fds: %d\n", inet_ntop(AF_INET, &(tcp_addr2.sin_addr),
			buffer, sizeof(buffer)), ntohs(tcp_addr2.sin_port), conn_fd, max_fds);
	    }
	    FD_SET(conn_fd, &all_fds);		/* add new descriptor to set */
	    max_fds = MAX(conn_fd, max_fds);	/* for select */
	    max_tcp_i = MAX(i, max_tcp_i);	/* max index in tcp_client[] array */
	}

	/* check for already connected TCP clients */
	for (i = 0; i <= max_tcp_i; i++) {	/* check all clients for data */
	    tcp_socket = tcp_client[i];
	    if (tcp_socket < 0)
		continue;
	    /* printf("%s tcp packet received from client #%d  max_tcp_i:%d todo:%d\n", time_stamp(timestamp), i, max_tcp_i,nready); */
	    if (FD_ISSET(tcp_socket, &read_fds)) {
		if (cs2_config_data.verbose && !background) {
		    time_stamp(timestamp);
		    printf("%s packet from: %s\n", timestamp, inet_ntop(AF_INET, &tcp_addr.sin_addr, buffer, sizeof(buffer)));
		}
		n = read(tcp_socket, netframe, CAN_ENCAP_SIZE);
		if (!n) {
		    /* connection closed by client */
		    if (cs2_config_data.verbose && !background) {
			time_stamp(timestamp);
			printf("%s client %s closed connection\n", timestamp,
			       inet_ntop(AF_INET, &tcp_addr.sin_addr, buffer, sizeof(buffer)));
		    }
		    close(tcp_socket);
		    FD_CLR(tcp_socket, &all_fds);
		    tcp_client[i] = -1;
		} else {
		    if (n % CAN_ENCAP_SIZE) {
			time_stamp(timestamp);
			fprintf(stderr, "%s received packet %% 13 : length %d\n", timestamp, n);
		    } else {
			/* check if we need to forward the message to CAN */
			if (!check_data(tcp_socket, &cs2_config_data, netframe)) {
			    ret = frame_to_can(sc, netframe);
			    if (!ret) {
				if (i > 0)
				    print_can_frame(TCP_FORMATS_STRG, netframe, cs2_config_data.verbose & !background);
				else
				    print_can_frame(TCP_FORMAT_STRG, netframe, cs2_config_data.verbose & !background);
			    }
			    net_to_net(sb, (struct sockaddr *)&baddr, netframe, CAN_ENCAP_SIZE);
			    print_can_frame(UDP_FORMAT_STRG, netframe, cs2_config_data.verbose & !background);
			}
		    }
		}
		if (--nready <= 0)
		    break;	/* no more readable descriptors */
	    }
	}
    }
    close(sc);
    close(sa);
    close(sb);
    close(st);
    close(st2);
    return 0;
}