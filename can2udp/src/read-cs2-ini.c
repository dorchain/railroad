/* ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <info@gerhard-bertelsmann.de> wrote this file. As long as you retain this
 * notice you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return
 * Gerhard Bertelsmann
 * ----------------------------------------------------------------------------
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <assert.h>
#include <libgen.h>

#include "read-cs2-config.h"

extern struct track_page_t *track_page;
extern struct track_data_t *track_data;
extern struct loco_data_t *loco_data;

int main(int argc, char **argv) {
    struct config_data_t config_data;
    char *dir, *var_dir;
    char *track_file;
    char *loco_file;

    var_dir = calloc(MAXDIR, 1);
    if (var_dir == NULL) {
	fprintf(stderr, "can't alloc bufer for directory string: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }

    if (argc == 2) {
	strncpy(var_dir, argv[1], MAXDIR - 1);
	config_data.directory = var_dir;
	asprintf(&dir, "%s", var_dir);
    } else {
	fprintf(stderr, "usage: %s <dir> \n", basename(argv[0]));
	free(var_dir);
	exit(EXIT_FAILURE);
    }

    config_data.verbose = 1;

    track_file = calloc(1, strlen(track_name) + strlen(config_data.directory) + 2);
    strcpy(track_file, config_data.directory);
    if (track_file[strlen(track_file) - 1] != '/')
	strcat(track_file, "/");
    strcat(track_file, track_name);

    /* printf("gbn : >%s<\n", track_file); */

    config_data.name = calloc(strlen(gbs_default) +1, 1);
    strcpy(config_data.name, gbs_default);
    strcat(config_data.directory, track_dir);

    printf("reading track pages ...\n");
    read_track_config(track_file);
    sort_tp_by_id();
    print_pages();
    printf("reading track files ...\n");
    read_track_pages(config_data.directory);
    printf("sorting track data ...\n");
    sort_td_by_id();
    /* print_tracks(); */
    /* print_gbstats(); */

    asprintf(&loco_file, "%s/%s", dir, loco_name);
    read_loco_data(loco_file);

    printf("\n\ntrack pages: %u\n", HASH_COUNT(track_page));
    printf("track data elements: %u\n", HASH_COUNT(track_data));
    printf("loco data: %u\n", HASH_COUNT(loco_data));

    delete_all_track_pages();
    delete_all_track_data();
    delete_all_loco_data();

    free(loco_file);
    free(dir);
    free(var_dir);
    free(track_file);
    printf("sizeof(loco_data_t): %ld\n", sizeof(struct loco_data_t));
    printf("sizeof(track_data_t): %ld\n", sizeof(struct track_data_t));
    return EXIT_SUCCESS;
}