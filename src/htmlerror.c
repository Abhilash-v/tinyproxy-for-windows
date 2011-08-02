/* $Id$
 * 
 * This file contains source code for the handling and display of
 * HTML error pages with variable substitution.
 *
 * Copyright (C) 2003  Steven Young <sdyoung@well.com>
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

#include "tinyproxy.h"

#include "common.h"
#include "buffer.h"
#include "conns.h"
#include "heap.h"
#include "htmlerror.h"
#include "network.h"
#include "utils.h"

/*
 * Add an error number -> filename mapping to the errorpages list.
 */
#define ERRORNUM_BUFSIZE 8     /* this is more than required */
#define ERRPAGES_BUCKETCOUNT 16

int
add_new_errorpage(char *filepath, unsigned int errornum) {
	char errornbuf[ERRORNUM_BUFSIZE];

	config.errorpages = hashmap_create(ERRPAGES_BUCKETCOUNT);
	if (!config.errorpages)
		return(-1);

	snprintf(errornbuf, ERRORNUM_BUFSIZE, "%u", errornum);

	if (hashmap_insert(config.errorpages, errornbuf,
			   filepath, strlen(filepath) + 1) < 0)
		return(-1);

	return(0);
}

/*
 * Get the file appropriate for a given error.
 */
static char*
get_html_file(unsigned int errornum) {
	hashmap_iter result_iter;
	char errornbuf[ERRORNUM_BUFSIZE];
	char *key;
	static char *val;

	assert(errornum >= 100 && errornum < 1000);

	if (!config.errorpages) return(config.errorpage_undef);

	snprintf(errornbuf, ERRORNUM_BUFSIZE, "%u", errornum);

	result_iter = hashmap_find(config.errorpages, errornbuf);

	if (hashmap_is_end(config.errorpages, result_iter))
		return(config.errorpage_undef);

	if (hashmap_return_entry(config.errorpages, result_iter,
				 &key, (void **)&val) < 0)
		return(config.errorpage_undef);

	return(val);
}

/*
 * Look up the value for a variable.
 */
static char*
lookup_variable(struct conn_s *connptr, char *varname) {
	hashmap_iter result_iter;
	char *key;
	static char *data;

	result_iter = hashmap_find(connptr->error_variables, varname);

	if (hashmap_is_end(connptr->error_variables, result_iter))
		return(NULL);

	if (hashmap_return_entry(connptr->error_variables, result_iter,
				 &key, (void **)&data) < 0)
		return(NULL);

	return(data);
}

#define HTML_BUFSIZE 4096

/*
 * Send an already-opened file to the client with variable substitution.
 */
int
send_html_file(FILE *infile, struct conn_s *connptr) {
	char inbuf[HTML_BUFSIZE], *varstart = NULL, *p;
	char *varval;
	int in_variable = 0, writeret;

	while(fgets(inbuf, HTML_BUFSIZE, infile) != NULL) {
		for (p = inbuf; *p; p++) {
			switch(*p) {
				case '}':
					if(in_variable) {
						*p = '\0';
						if(!(varval = lookup_variable(connptr, varstart)))
							varval = "(unknown)";
						writeret = write_message(connptr->client_fd, "%s",
												 varval);
						if(writeret) return(writeret);
						in_variable = 0;
					} else {
						writeret = write_message(connptr->client_fd, "%c", *p);
						if (writeret) return(writeret);
					}
					break;
				case '{':
					/* a {{ will print a single {.  If we are NOT
					 * already in a { variable, then proceed with
					 * setup.  If we ARE already in a { variable,
					 * this code will fallthrough to the code that
					 * just dumps a character to the client fd.
					 */
					 if(!in_variable) {
					 	varstart = p+1;
						in_variable++;
					} else
						in_variable = 0;
				default:
					if(!in_variable) {
						writeret = write_message(connptr->client_fd, "%c", 
												 *p);
						if(writeret) return(writeret);
					}
		
			}
		}
		in_variable = 0;
	}
	return(0);
}

int
send_http_headers(struct conn_s *connptr, int code, char *message) {
	char *headers = \
		"HTTP/1.0 %d %s\r\n" \
		"Server: %s/%s\r\n" \
		"Content-Type: text/html\r\n" \
		"Connection: close\r\n" \
		"\r\n";

	return(write_message(connptr->client_fd, headers,
		   				 code, message,
		   				 PACKAGE, VERSION));
}

/*
 * Display an error to the client.
 */
int
send_http_error_message(struct conn_s *connptr)
{
	char *error_file;
	FILE *infile;
	int ret;
	char *fallback_error = \
		"<html><head><title>%s</title></head>" \
		"<body><blockquote><i>%s %s</i><br>" \
		"The page you requested was unavailable. The error code is listed " \
		"below. In addition, the HTML file which has been configured as the " \
		"page to be displayed when an error of this type was unavailable, " \
		"with the error code %d (%s).  Please contact your administrator." \
		"<center>%s</center>" \
		"</body></html>" \
        "\r\n";

	send_http_headers(connptr, connptr->error_number, connptr->error_string);

	error_file = get_html_file(connptr->error_number);
	if(!(infile = fopen(error_file, "r"))) 
		return(write_message(connptr->client_fd, fallback_error,
							 connptr->error_string,
							 PACKAGE, VERSION,
							 errno, strerror(errno),
							 connptr->error_string));

	ret = send_html_file(infile, connptr);
	fclose(infile);
	return(ret);
}

/* 
 * Add a key -> value mapping for HTML file substitution.
 */

#define ERRVAR_BUCKETCOUNT 16

int 
add_error_variable(struct conn_s *connptr, char *key, char *val) 
{
	if(!connptr->error_variables)
		if (!(connptr->error_variables = hashmap_create(ERRVAR_BUCKETCOUNT)))
			return(-1);

	if (hashmap_insert(connptr->error_variables, key, val,
			   strlen(val) + 1) < 0)
		return(-1);

	return(0);
}

#define ADD_VAR_RET(x, y) if(y) { if(add_error_variable(connptr, x, y) == -1) return(-1); }

/*
 * Set some standard variables used by all HTML pages
 */
int
add_standard_vars(struct conn_s *connptr) {
	char timebuf[30];
	time_t global_time = time(NULL);

	strftime(timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT",
			 gmtime(&global_time));

	ADD_VAR_RET("request", connptr->request_line);
	ADD_VAR_RET("cause", connptr->error_string);
	ADD_VAR_RET("clientip", connptr->client_ip_addr);
	ADD_VAR_RET("clienthost", connptr->client_string_addr);
	ADD_VAR_RET("version", VERSION);
	ADD_VAR_RET("package", PACKAGE);
	ADD_VAR_RET("date", timebuf);
	return(0);
}

/*
 * Add the error information to the conn structure.
 */
int
indicate_http_error(struct conn_s* connptr, int number, char *message, ...)
{
	va_list ap;
	char *key, *val;

	va_start(ap, message);

	while((key = va_arg(ap, char *))) {
		val = va_arg(ap, char *);
		if(add_error_variable(connptr, key, val) == -1) {
			va_end(ap);
			return(-1);
		}
	}

	connptr->error_number = number;
	connptr->error_string = safestrdup(message);

	va_end(ap);

	return(add_standard_vars(connptr));
}
