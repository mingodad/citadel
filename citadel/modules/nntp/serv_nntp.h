//
// Header file for NNTP server module
//
// Copyright (c) 2014 by the citadel.org team
//
// This program is open source software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 3.
//  
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//


// data returned by a message list fetch
struct nntp_msglist {
	int num_msgs;
	long *msgnums;
};


// data passed by the LIST commands to its helper function
struct nntp_list_data {
	int list_format;
	char *wildmat_pattern;
};


//
// data passed between nntp_listgroup() and nntp_listgroup_backend()
//
struct listgroup_range {
	long lo;
	long hi;
};


typedef struct _citnntp {		// Information about the current session
	long current_article_number;
} citnntp;


//
// Various output formats for the LIST commands
//
enum {
	NNTP_LIST_ACTIVE,
	NNTP_LIST_ACTIVE_TIMES,
	NNTP_LIST_DISTRIB_PATS,
	NNTP_LIST_HEADERS,
	NNTP_LIST_NEWSGROUPS,
	NNTP_LIST_OVERVIEW_FMT
};


int wildmat(const char *text, const char *p);

