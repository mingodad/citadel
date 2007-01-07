/**
 *	Ugly hack to make the non-automake build work without source changes
 */

#define _GNU_SOURCE
#define RUNDIR		WEBCITDIR
#define BASEDIR		WEBCITDIR
#define DATADIR		WEBCITDIR
#define PREFIX		WEBCITDIR
