/*
 * netsetup.c
 *
 * Copyright (c) 1998  Art Cancro
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "citadel.h"

struct roomshare {
	struct roomshare *next;
	char rs_name[30];
	long rs_lastsent;
	};

struct netnode {
	char nn_nodename[32];
	char nn_spoolcmd[256];
	struct roomshare *nn_first;
	};


void get_config();
struct config config;


struct netnode *load_node(nodename)
char *nodename; {
	FILE *fp;
	char buf[256];
	char filename[256];
	struct netnode *newnn;
	struct roomshare *newrs;

	sprintf(filename, "./network/systems/%s", nodename);
	fp = fopen(filename, "r");
	if (fp == NULL) {
		return NULL;
		}

	newnn = (struct netnode *) malloc(sizeof(struct netnode));
	strcpy(newnn->nn_nodename, nodename);
	newnn->nn_first = NULL;

	fgets(buf, 255, fp);
	buf[strlen(buf)-1] = 0;
	strcpy(newnn->nn_spoolcmd, buf);

	while (fgets(buf, 255, fp) != NULL) {
		newrs = (struct roomshare *) malloc(sizeof(struct roomshare));
		newrs->next = newnn->nn_first;
		newnn->nn_first = newrs;
		buf[strlen(buf)-1] = 0;
		strcpy(newrs->rs_name, buf);
		fgets(buf, 255, fp);
		buf[strlen(buf)-1] = 0;
		newrs->rs_lastsent = atol(buf);
		}

	fclose(fp);
	return(newnn);
	}



void save_node(nnptr)
struct netnode *nnptr; {

	FILE *fp;
	char filename[256];
	struct roomshare *rsptr = NULL;
	
	sprintf(filename, "./network/systems/%s", nnptr->nn_nodename);
	fp = fopen(filename, "w");
	if (fp == NULL) {
		fprintf(stderr, "%s\n", strerror(errno));
		return;
		}
	fprintf(fp, "%s\n", nnptr->nn_spoolcmd);
	while (nnptr->nn_first != NULL) {
		fprintf(fp, "%s\n%ld\n", nnptr->nn_first->rs_name,
					nnptr->nn_first->rs_lastsent);
		rsptr = nnptr->nn_first->next;
		free(nnptr->nn_first);
		nnptr->nn_first = rsptr;
		}
	fclose(fp);
	free(rsptr);
	}



void display_usage() {
	fprintf(stderr, "netsetup for %s\n", CITADEL);
	fprintf(stderr, "usage: netsetup <command> [arguments]\n\n");
	fprintf(stderr, "Commands: \n");
	fprintf(stderr, "   nodelist                  (Lists all neighboring nodes\n");
	fprintf(stderr, "   addnode [name]            (Adds a new node to the list)\n");
	fprintf(stderr, "   deletenode [name]         (Deletes a node from the list)\n");
	fprintf(stderr, "   roomlist [node]           (List rooms being shared)\n");
	fprintf(stderr, "   getcommand [node]         (Show spool command)\n");
	fprintf(stderr, "   setcommand [node] [cmd]   (Set spool command)\n");
	fprintf(stderr, "   share [node] [room]       (Add a new shared room)\n");
	fprintf(stderr, "   unshare [node] [room]     (Stop sharing a room)\n");
	fprintf(stderr, "   help                      (Display this message)\n");
	}


/*
 * Display all neighboring nodes
 * (This is inherently nonportable)
 */
void display_nodelist() {
	FILE *ls;
	char buf[256];

	ls = (FILE *) popen("cd ./network/systems; ls", "r");
	if (ls == NULL) {
		fprintf(stderr, "netsetup: Cannot open nodelist: %s\n",
			strerror(errno));
		exit(errno);
		}

	while (fgets(buf, 255, ls) != NULL) {
		printf("%s", buf);
		}

	pclose(ls);
	}



/*
 */
void add_node(NewNodeName)
char *NewNodeName; {
	FILE *fp;
	char sysfilename[256];

	sprintf(sysfilename, "./network/systems/%s", NewNodeName);

	fp = fopen(sysfilename, "r");
	if (fp != NULL) {
		fclose(fp);
		fprintf(stderr, "A node named '%s' already exists.\n",
			NewNodeName);
		exit(2);
		}

	fp = fopen(sysfilename, "w");
	if (fp == NULL) {
		fprintf(stderr, "%s\n", strerror(errno));
		exit(errno);
		}

	fprintf(fp, "cat %%s >>./network/spoolout/%s\n", NewNodeName);
	fclose(fp);
	}


/*
 */
void delete_node(NodeName)
char *NodeName; {
	FILE *fp;
	char sysfilename[256];
	char spooloutfilename[256];

	sprintf(sysfilename, "./network/systems/%s", NodeName);
	sprintf(spooloutfilename, "./network/spoolout/%s", NodeName);

	fp = fopen(sysfilename, "r");
	if (fp == NULL) {
		fprintf(stderr, "'%s' does not exist.\n",
			NodeName);
		exit(3);
		}
	fclose(fp);

	unlink(spooloutfilename);
	if (unlink(sysfilename)==0) {
		return;
		}
	fprintf(stderr, "%s\n", strerror(errno));
	exit(errno);
	}


/*
 */
void do_roomlist(NodeName)
char *NodeName; {
	FILE *fp;
	char sysfilename[256];
	char buf[256];

	sprintf(sysfilename, "./network/systems/%s", NodeName);

	fp = fopen(sysfilename, "r");
	if (fp == NULL) {
		fprintf(stderr, "'%s' does not exist.\n",
			NodeName);
		exit(3);
		}

	fgets(buf, 255, fp);	/* skip past spool cmd */
	while (fgets(buf, 255, fp) != NULL) {
		printf("%s", buf);
		fgets(buf, 255, fp);	/* skip past last-sent pointer */
		}

	fclose(fp);
	}



/*
 */
void show_spool_cmd(NodeName)
char *NodeName; {
	FILE *fp;
	char sysfilename[256];
	char buf[256];

	sprintf(sysfilename, "./network/systems/%s", NodeName);

	fp = fopen(sysfilename, "r");
	if (fp == NULL) {
		fprintf(stderr, "'%s' does not exist.\n",
			NodeName);
		exit(3);
		}

	fgets(buf, 255, fp);
	printf("%s", buf);
	fclose(fp);
	}


/*
 */
void set_spool_cmd(nodename, spoolcmd)
char *nodename;
char *spoolcmd; {
	struct netnode *nnptr;

	nnptr = load_node(nodename);
	if (nnptr == NULL) {
		fprintf(stderr, "No such node '%s'.\n", nodename);
		exit(4);
		}

	strncpy(nnptr->nn_spoolcmd, spoolcmd, 255);
	save_node(nnptr);
	}


/*
 */
void add_share(nodename, roomname)
char *nodename;
char *roomname; {
	struct netnode *nnptr;
	struct roomshare *rsptr;
	long highest = 0L;
	int foundit = 0;

	nnptr = load_node(nodename);
	if (nnptr == NULL) {
		fprintf(stderr, "No such node '%s'.\n", nodename);
		exit(4);
		}

	for (rsptr = nnptr->nn_first; rsptr != NULL; rsptr = rsptr->next) {
		if (!strcasecmp(rsptr->rs_name, roomname)) {
			foundit = 1;
			}
		if (rsptr->rs_lastsent > highest) {
			highest = rsptr->rs_lastsent;
			}
		}

	if (foundit == 0) {
		rsptr = (struct roomshare *) malloc(sizeof(struct roomshare));
		rsptr->next = nnptr->nn_first;
		strcpy(rsptr->rs_name, roomname);
		rsptr->rs_lastsent = highest;
		nnptr->nn_first = rsptr;
		}

	save_node(nnptr);
	}


/*
 */
void remove_share(nodename, roomname)
char *nodename;
char *roomname; {
	struct netnode *nnptr;
	struct roomshare *rsptr, *rshold;
	int foundit = 0;

	nnptr = load_node(nodename);
	if (nnptr == NULL) {
		fprintf(stderr, "No such node '%s'.\n", nodename);
		exit(4);
		}

	if (nnptr->nn_first != NULL)
	   if (!strcasecmp(nnptr->nn_first->rs_name, roomname)) {
		rshold = nnptr->nn_first;
		nnptr->nn_first = nnptr->nn_first->next;
		free(rshold);
		foundit = 1;
		}

	if (nnptr->nn_first != NULL)
	   for (rsptr = nnptr->nn_first; rsptr->next != NULL; rsptr = rsptr->next) {
		if (!strcasecmp(rsptr->next->rs_name, roomname)) {
			rshold = rsptr->next;
			rsptr->next = rsptr->next->next;
			free(rshold);
			foundit = 1;
			rsptr = nnptr->nn_first;
			}
		}

	save_node(nnptr);

	if (foundit == 0) {
		fprintf(stderr, "Not sharing '%s' with %s\n",
			roomname, nodename);
		exit(5);
		}
	}


int main(argc, argv)
int argc;
char *argv[]; {

	if (argc < 2) {
		display_usage();
		exit(1);
		}

	get_config();

	if (!strcmp(argv[1], "help")) {
		display_usage();
		exit(0);
		}

	if (!strcmp(argv[1], "nodelist")) {
		display_nodelist();
		exit(0);
		}

	if (!strcmp(argv[1], "addnode")) {
		if (argc < 3) {
			display_usage();
			exit(1);
			}
		add_node(argv[2]);
		exit(0);
		}

	if (!strcmp(argv[1], "deletenode")) {
		if (argc < 3) {
			display_usage();
			exit(1);
			}
		delete_node(argv[2]);
		exit(0);
		}

	if (!strcmp(argv[1], "roomlist")) {
		if (argc < 3) {
			display_usage();
			exit(1);
			}
		do_roomlist(argv[2]);
		exit(0);
		}

	if (!strcmp(argv[1], "getcommand")) {
		if (argc < 3) {
			display_usage();
			exit(1);
			}
		show_spool_cmd(argv[2]);
		exit(0);
		}

	if (!strcmp(argv[1], "setcommand")) {
		if (argc < 4) {
			display_usage();
			exit(1);
			}
		set_spool_cmd(argv[2], argv[3]);
		exit(0);
		}

	if (!strcmp(argv[1], "share")) {
		if (argc < 4) {
			display_usage();
			exit(1);
			}
		add_share(argv[2], argv[3]);
		exit(0);
		}

	if (!strcmp(argv[1], "unshare")) {
		if (argc < 4) {
			display_usage();
			exit(1);
			}
		remove_share(argv[2], argv[3]);
		exit(0);
		}

	display_usage();
	exit(1);
	}
