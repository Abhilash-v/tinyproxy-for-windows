/* $Id$
 *
 * See 'tinyproxy.c' for a detailed description.
 *
 * Copyright (C) 1998  Steven Young
 * Copyright (C) 1999  Robert James Kaes (rjkaes@flarenet.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef TINYPROXY_TINYPROXY_H
#define TINYPROXY_TINYPROXY_H
#define in_addr_t unsigned int
#include "common.h"
#include "hashmap.h"

/* Global variables for the main controls of the program */
#define MAXBUFFSIZE	((size_t)(1024 * 96))	/* Max size of buffer */
#define MAX_IDLE_TIME 	(60 * 10)	/* 10 minutes of no activity */

#ifdef UPSTREAM_SUPPORT
struct upstream {
	struct upstream *next;
	char *domain; /* optional */
	char *host;
	int port;
	in_addr_t ip, mask;
};
#endif

#ifdef REVERSE_SUPPORT
struct reversepath {
	struct reversepath *next;
	char *path;
	char *url;
};

#define REVERSE_COOKIE "yummy_magical_cookie"
#endif

/*
 * Hold all the configuration time information.
 */
struct config_s {
	char *logf_name;
	char *config_file;
	unsigned int syslog; /* boolean */
	int port;
	char *stathost;
	unsigned int quit; /* boolean */
	char *username;
	char *group;
	char *ipAddr;
#ifdef FILTER_ENABLE
	char *filter;
	unsigned int filter_url; /* boolean */
	unsigned int filter_extended; /* boolean */
        unsigned int filter_casesensitive; /* boolean */
#endif				/* FILTER_ENABLE */
#ifdef XTINYPROXY_ENABLE
	char *my_domain;
#endif
#ifdef REVERSE_SUPPORT
	struct reversepath *reversepath_list;
	unsigned int reverseonly; /* boolean */
	unsigned int reversemagic; /* boolean */
	char *reversebaseurl;
#endif
#ifdef UPSTREAM_SUPPORT
	struct upstream *upstream_list;
#endif				/* UPSTREAM_SUPPORT */
	char *pidpath;
	unsigned int idletimeout;
	char* bind_address;

	/*
	 * The configured name to use in the HTTP "Via" header field.
	 */
	char* via_proxy_name;

        /* 
	 * Error page support.  Map error numbers to file paths.
	 */
	hashmap_t errorpages;

	/* 
	 * Error page to be displayed if appropriate page cannot be located
	 * in the errorpages structure.
	 */
	char *errorpage_undef;

	/* 
	 * The HTML statistics page. 
	 */
	char *statpage;
};

/* Global Structures used in the program */
extern struct config_s config;
extern unsigned int received_sighup; /* boolean */
extern unsigned int processed_config_file; /* boolean */

#endif
