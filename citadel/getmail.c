/*
 * $Id: sendcommand.c 5736 2007-11-10 23:12:19Z dothebart $
 *
 * Command-line utility to transmit a server command.
 *
 */


#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <libcitadel.h>
#include "citadel.h"
#include "citadel_ipc.h"
#include "server.h"
#include "config.h"

#define LOCKFILE "/tmp/LCK.sendcommand"

static CtdlIPC *ipc = NULL;

/*
 * make sure only one copy of sendcommand runs at a time, using lock files
 */
int set_lockfile(void)
{
	FILE *lfp;
	int onppid;

	if ((lfp = fopen(LOCKFILE, "r")) != NULL) {
		fscanf(lfp, "%d", &onppid);
		fclose(lfp);
		if (!kill(onppid, 0) || errno == EPERM)
			return 1;
	}
	lfp = fopen(LOCKFILE, "w");
	fprintf(lfp, "%ld\n", (long) getpid());
	fclose(lfp);
	return (0);
}

void remove_lockfile(void)
{
	unlink(LOCKFILE);
}

/*
 * Why both cleanup() and nq_cleanup() ?  Notice the alarm() call in
 * cleanup() .  If for some reason sendcommand hangs waiting for the server
 * to clean up, the alarm clock goes off and the program exits anyway.
 * The cleanup() routine makes a check to ensure it's not reentering, in
 * case the ipc module looped it somehow.
 */
void nq_cleanup(int e)
{
	remove_lockfile();
	exit(e);
}

/*
 * send binary to server
 */
void serv_write(CtdlIPC *ipc, const char *buf, unsigned int nbytes)
{
	unsigned int bytes_written = 0;
	int retval;
/*
#if defined(HAVE_OPENSSL)
	if (ipc->ssl) {
		serv_write_ssl(ipc, buf, nbytes);
		return;
	}
#endif
*/
	while (bytes_written < nbytes) {
		retval = write(ipc->sock, &buf[bytes_written],
			       nbytes - bytes_written);
		if (retval < 1) {
			connection_died(ipc, 0);
			return;
		}
		bytes_written += retval;
	}
}


void cleanup(int e)
{
	static int nested = 0;

	alarm(30);
	signal(SIGALRM, nq_cleanup);
	serv_write(ipc, "\n", 1);
	if (nested++ < 1)
		CtdlIPCQuit(ipc);
	nq_cleanup(e);
}

/*
 * This is implemented as a function rather than as a macro because the
 * client-side IPC modules expect logoff() to be defined.  They call logoff()
 * when a problem connecting or staying connected to the server occurs.
 */
void logoff(int e)
{
	cleanup(e);
}

static char *args[] =
{"getmail", NULL};

/*
 * Connect sendcommand to the Citadel server running on this computer.
 */
void np_attach_to_server(char *host, char *port)
{
	char buf[SIZ];
	char hostbuf[256] = "";
	char portbuf[256] = "";
	int r;

	fprintf(stderr, "Attaching to server...\n");
	strncpy(hostbuf, host, 256);
	strncpy(portbuf, port, 256);
	ipc = CtdlIPC_new(1, args, hostbuf, portbuf);
	if (!ipc) {
		fprintf(stderr, "Can't connect: %s\n", strerror(errno));
		exit(3);
	}
	CtdlIPC_chat_recv(ipc, buf);
	fprintf(stderr, "%s\n", &buf[4]);
	snprintf(buf, sizeof buf, "IPGM %d", config.c_ipgm_secret);
	r = CtdlIPCInternalProgram(ipc, config.c_ipgm_secret, buf);
	fprintf(stderr, "%s\n", buf);
	if (r / 100 != 2) {
		cleanup(2);
	}
}


void sendcommand_die(void) {
	exit(0);
}


/*
 * saves filelen bytes from file at pathname
 */
int save_buffer(void *file, size_t filelen, const char *pathname)
{
	size_t block = 0;
	size_t bytes_written = 0;
	FILE *fp;

	fp = fopen(pathname, "w");
	if (!fp) {
		fprintf(stderr, "Cannot open '%s': %s\n", pathname, strerror(errno));
		return 0;
	}
	do {
		block = fwrite((char *)file + bytes_written, 1,
				filelen - bytes_written, fp);
		bytes_written += block;
	} while (errno == EINTR && bytes_written < filelen);
	fclose(fp);

	if (bytes_written < filelen) {
		fprintf(stderr,"Trouble saving '%s': %s\n", pathname,
				strerror(errno));
		return 0;
	}
	return 1;
}


/*
 * main
 */
int main(int argc, char **argv)
{
	int a, r, i;
	char cmd[5][SIZ];
	char buf[SIZ];
	int MessageToRetrieve;
	int MessageFound = 0;
	int relh=0;
	int home=0;
	int n=0;
	char relhome[PATH_MAX]="";
	char ctdldir[PATH_MAX]=CTDLDIR;
	fd_set read_fd;
	struct timeval tv;
	int ret, err;
	int server_shutting_down = 0;
	struct ctdlipcroom *Room;
	struct ctdlipcmessage *mret;
	char cret[SIZ];
	unsigned long *msgarr;
	struct parts *att;

	strcpy(ctdl_home_directory, DEFAULT_PORT);

	/*
	 * Change directories if specified
	 */
	for (a = 1; a < argc && n < 5; ++a) {
		if (!strncmp(argv[a], "-h", 2)) {
			relh=argv[a][2]!='/';
			if (!relh) safestrncpy(ctdl_home_directory, &argv[a][2],
								   sizeof ctdl_home_directory);
			else
				safestrncpy(relhome, &argv[a][2],
							sizeof relhome);
			home=1;
		} else {

			strcpy(cmd[n++], argv[a]);
		}
	}

	calc_dirs_n_files(relh, home, relhome, ctdldir, 0);
	get_config();

	signal(SIGINT, cleanup);
	signal(SIGQUIT, cleanup);
	signal(SIGHUP, cleanup);
	signal(SIGTERM, cleanup);

	fprintf(stderr, "getmail: started (pid=%d) "
			"running in %s\n",
			(int) getpid(),
			ctdl_home_directory);
	fflush(stderr);

//	alarm(5);
//	signal(SIGALRM, nq_cleanup); /* Set up a watchdog type timer in case we hang */
	
	np_attach_to_server(UDS, ctdl_run_dir);
	fflush(stderr);
	setIPCDeathHook(sendcommand_die);

	fprintf(stderr, "GOTO %s\n", cmd[0]);
	CtdlIPCGotoRoom(ipc, cmd[0], "", &Room, cret);
	fprintf(stderr, "%s\n", cret);

	MessageToRetrieve = atol(cmd[1]);

	r = CtdlIPCGetMessages(ipc, 0, 0, NULL, &msgarr, buf);
	printf("Messages: ");
	for (i = 0; msgarr[i] > 0 ; i ++)
	{
//		printf(" %ld ", msgarr[i]);
		if (msgarr[i] == MessageToRetrieve)
			MessageFound = 1;
	}
	if (!MessageFound)
		printf("Message %d not found in the above list.", MessageToRetrieve);
	printf("\n");

	CtdlIPCGetSingleMessage(ipc,  MessageToRetrieve,0,4, &mret, cret);
	fprintf(stderr, "%s\n", cret);
	fprintf(stderr, "%s: %s\n", "path", mret->path);
	fprintf(stderr, "%s: %s\n", "author", mret->author);
	fprintf(stderr, "%s: %s\n", "subject", mret->subject);
	fprintf(stderr, "%s: %s\n", "email", mret->email);
	fprintf(stderr, "%s: %s\n", "text", mret->text);

	att = mret->attachments;

	while (att != NULL){
		void *attachment;
		char tmp[PATH_MAX];
		char buf[SIZ];

		fprintf(stderr, "Attachment: [%s] %s\n", att->number, att->filename);
		r = CtdlIPCAttachmentDownload(ipc, MessageToRetrieve, att->number, &attachment, NULL, buf);
		printf("----\%s\n----\n", buf);
		if (r / 100 != 2) {
			printf("%s\n", buf);
		} else {
			size_t len;
			
			len = (size_t)extract_long(buf, 0);
			CtdlMakeTempFileName(tmp, sizeof tmp);
			strcat(tmp, att->filename);
			printf("Saving Attachment to %s", tmp);
			save_buffer(attachment, len, tmp);
			free(attachment);
		}
		att = att->next;

	}

	///if (


	CtdlIPCQuit(ipc);
	exit (1);






	CtdlIPC_chat_send(ipc, cmd[4]);
	CtdlIPC_chat_recv(ipc, buf);
	fprintf(stderr, "%s\n", buf);

	tv.tv_sec = 0;
	tv.tv_usec = 1000;

	if (!strncasecmp(&buf[1], "31", 2)) {
		server_shutting_down = 1;
	}

	if (buf[0] == '1') {
		while (CtdlIPC_chat_recv(ipc, buf), strcmp(buf, "000")) {
			printf("%s\n", buf);
			alarm(5); /* Kick the watchdog timer */
		}
	} else if (buf[0] == '4') {
		do {
			if (fgets(buf, sizeof buf, stdin) == NULL)
				strcpy(buf, "000");
			if (!IsEmptyStr(buf))
				if (buf[strlen(buf) - 1] == '\n')
					buf[strlen(buf) - 1] = 0;
			if (!IsEmptyStr(buf))
				if (buf[strlen(buf) - 1] == '\r')
					buf[strlen(buf) - 1] = 0;
			if (strcmp(buf, "000"))
				CtdlIPC_chat_send(ipc, buf);
			
			FD_ZERO(&read_fd);
			FD_SET(ipc->sock, &read_fd);
			ret = select(ipc->sock+1, &read_fd, NULL, NULL,  &tv);
			err = errno;
			if (err!=0)
				printf("select failed: %d", err);

			if (ret == -1) {
				if (!(errno == EINTR || errno == EAGAIN))
					printf("select failed: %d", err);
				return 1;
			}

			if (ret != 0) {
				size_t n;
				char rbuf[SIZ];

				rbuf[0] = '\0';
				n = read(ipc->sock, rbuf, SIZ);
				if (n>0) {
					rbuf[n]='\0';
					fprintf (stderr, rbuf);
					fflush (stdout);
				}
			}
			alarm(5); /* Kick the watchdog timer */
		} while (strcmp(buf, "000"));
		CtdlIPC_chat_send(ipc, "\n");
		CtdlIPC_chat_send(ipc, "000");
	}
	alarm(0);	/* Shutdown the watchdog timer */
	fprintf(stderr, "sendcommand: processing ended.\n");

	/* Clean up and log off ... unless the server indicated that the command
	 * we sent is shutting it down, in which case we want to just cut the
	 * connection and exit.
	 */
	if (server_shutting_down) {
		nq_cleanup(0);
	}
	else {
		cleanup(0);
	}
	return 0;
}
