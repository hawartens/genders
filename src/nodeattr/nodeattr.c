/*****************************************************************************\
 *  $Id: nodeattr.c,v 1.42 2010-02-02 00:04:34 chu11 Exp $
 *****************************************************************************
 *  Copyright (C) 2007-2010 Lawrence Livermore National Security, LLC.
 *  Copyright (C) 2001-2007 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Jim Garlick <garlick@llnl.gov> and Albert Chu <chu11@llnl.gov>.
 *  UCRL-CODE-2003-004.
 *  
 *  This file is part of Genders, a cluster configuration database.
 *  For details, see <http://www.llnl.gov/linux/genders/>.
 *  
 *  Genders is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *  
 *  Genders is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with Genders.  If not, see <http://www.gnu.org/licenses/>.
\*****************************************************************************/

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#if STDC_HEADERS
#include <string.h>
#endif /* STDC_HEADERS */
#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if HAVE_GETOPT_H 
#include <getopt.h>
#endif /* HAVE_GETOPT_H */

#include "genders.h"
#include "hostlist.h"

#if HAVE_GETOPT_LONG
#define GETOPT(ac,av,opt,lopt) getopt_long(ac,av,opt,lopt,NULL)
#else
#define GETOPT(ac,av,opt,lopt) getopt(ac,av,opt)
#endif

#define OPTIONS "cnsqX:AvQVUlf:kd:"

#if HAVE_GETOPT_LONG
static struct option longopts[] = {
    { "querycomma", 0, 0, 'c' },
    { "querynl", 0, 0, 'n' },
    { "queryspace", 0, 0, 'n' },
    { "query", 0, 0, 'q' },
    { "excludequery", 1, 0, 'X'},
    { "allnodes", 0, 0, 'A' },
    { "value", 0, 0, 'v' },
    { "testquery", 0, 0, 'Q' },
    { "values", 0, 0, 'V' },
    { "unique", 0, 0, 'U' },
    { "listattr", 0, 0, 'l' },
    { "filename", 1, 0, 'f' },
    { "parse-check", 0, 0, 'k'},
    { "diff", 1, 0, 'd'},
    { 0,0,0,0 },
};
#endif

typedef enum { FMT_COMMA, FMT_NL, FMT_SPACE, FMT_HOSTLIST } fmt_t;

static int test_attr(genders_t gp, char *node, char *attr, int vopt);
static int test_query(genders_t gp, char *node, char *query);
static void list_attr_val(genders_t gp, char *attr, int Uopt);
static void list_nodes(genders_t gp, char *attr, char *excludequery, fmt_t fmt);
static void list_attrs(genders_t gp, char *node);
static void usage(void);
static void diff_genders(char *db1, char *db2);

/* Utility functions */
static int _gend_error_exit(genders_t gp, char *msg);
static void *_safe_malloc(size_t size);
static void *_rangestr(hostlist_t hl, fmt_t fmt);
static char *_val_create(genders_t gp);
#if 0
static char *_to_gendname(genders_t gp, char *val);
static char *_node_create(genders_t gp);
static char *_attr_create(genders_t gp);
#endif

int
main(int argc, char *argv[])
{
    int c, errors;
    int Aopt = 0, lopt = 0, qopt = 0, Xopt = 0, vopt = 0, Qopt = 0,
      Vopt = 0, Uopt = 0, kopt = 0, dopt = 0;
    char *filename = GENDERS_DEFAULT_FILE;
    char *dfilename = NULL;
    char *excludequery = NULL;
    fmt_t qfmt = FMT_HOSTLIST;
    genders_t gp;

    while ((c = GETOPT(argc, argv, OPTIONS, longopts)) != EOF) {
        switch (c) {
        case 'c':   /* --querycomma */
            qfmt = FMT_COMMA;
            qopt = 1;
            break;
        case 'n':   /* --querynl */
            qfmt = FMT_NL;
            qopt = 1;
            break;
        case 's':   /* --queryspace */
            qfmt = FMT_SPACE;
            qopt = 1;
            break;
        case 'q':   /* --query */
            qfmt = FMT_HOSTLIST;
            qopt = 1;
            break;
        case 'X':   /* --excludequery */
            excludequery = optarg;
            Xopt = 1;
            break;
	case 'A':   /* --allnodes */
	    Aopt = 1;
	    break;
        case 'v':   /* --value */
            vopt = 1;
            break;
        case 'Q':   /* --testquery */
            Qopt = 1;
            break;
        case 'V':   /* --values */
            Vopt = 1;
            break;
        case 'U':   /* --unique */
            Uopt = 1;
            break;
        case 'l':   /* --listattr */
            lopt = 1;
            break;
        case 'f':   /* --filename */
            filename = optarg;
            break;
        case 'k':   /* --check */ 
            kopt = 1;
            break;
        case 'd':   /* --diff */
            dopt = 1;
            dfilename = optarg;
            break;
        default:
            usage();
            break;
        }
    }

    /* check parameter inputs */

    /* specify correct option combinations */
    if ((qopt + Qopt + Vopt + lopt + kopt + dopt) > 1)
        usage();

    if ((qopt || Qopt || Vopt || lopt || kopt || dopt) && vopt)
        usage();

    if (Aopt && !qopt)
        usage();

    if (!qopt && Xopt)
      usage();

    if (!Vopt && Uopt)
      usage();

    /* specified correctly number of arguments */
    if ((qopt 
	 && ((!Aopt && optind != (argc - 1))
	     || (Aopt && optind != argc))) 
        || (!qopt
            && !Qopt
            && !Vopt
            && !lopt
            && !kopt
            && !dopt
            && (optind != (argc - 1) && optind != (argc - 2)))
        || (Qopt && (optind != (argc - 1) && optind != (argc - 2)))
        || (Vopt && optind != (argc - 1))
        || (lopt && (optind != argc && optind != (argc - 1)))
        || (kopt && optind != argc)
        || (dopt && optind != argc))
      usage();

    /* genders database diff */
    if (dopt) {
        diff_genders(filename, dfilename);
        exit(0);
    }

    /* Initialize genders package. */
    gp = genders_handle_create();
    if (!gp) {
        fprintf(stderr, "nodeattr: out of memory\n");
        exit(1);
    }

    /* parse check */
    if (kopt) {
        errors = genders_parse(gp, filename, NULL);
        if (errors == -1 && genders_errnum(gp) != GENDERS_ERR_PARSE)
            _gend_error_exit(gp, "genders_parse");
        if (errors >= 0)
            fprintf(stderr, "nodeattr: %d parse errors discovered\n", errors);
        exit(errors);
    }

    if (genders_load_data(gp, filename) < 0)
        _gend_error_exit(gp, filename);

    /* Usage 1: list nodes with specified attribute, or all nodes */
    if (qopt) {
        char *query;

	if (Aopt)
	    list_nodes(gp, NULL, NULL, qfmt); 
	else {
	    query = argv[optind++];
	    list_nodes(gp, query, excludequery, qfmt);
	}

        exit(0);
    }

    /* Usage 2:  does node have attribute? */
    if (!qopt && !Qopt && !Vopt && !lopt && !kopt && !dopt) {
        char *node = NULL, *attr = NULL;
        int result;

        if (optind == argc - 2) {
            node = argv[optind++];
            attr = argv[optind++];
        } else {
            node = NULL;
            attr = argv[optind++];
        }

        result = test_attr(gp, node, attr, vopt);
        exit(result ? 0 : 1);
    }

    /* Usage 3:  does node meet query conditions */
    if (Qopt) {
        char *node = NULL, *query = NULL;
        int result;

        if (optind == argc - 2) {
            node = argv[optind++];
            query = argv[optind++];
        } else {
            node = NULL;
            query = argv[optind++];
        }

        result = test_query(gp, node, query);
        exit(result ? 0 : 1);
    }

    /* Usage 4:  output all attribute values */
    if (Vopt) {
        char *attr = NULL;

        attr = argv[optind++];

        if (strchr(attr, '='))  /* attr cannot be "attr=val" */
            usage();

        list_attr_val(gp, attr, Uopt);
    }

    /* Usage 6:  list attributes */
    if (lopt) {
        char *node = NULL;

        if (optind == argc - 1)
            node = argv[optind++];

        list_attrs(gp, node);
    }

    /*NOTREACHED*/
    exit(0);
}

static void 
list_nodes(genders_t gp, char *query, char *excludequery, fmt_t qfmt)
{
    char **nodes;
    int i, count;
    int len;
    hostlist_t hl;
    char *str;

    if ((len = genders_nodelist_create(gp, &nodes)) < 0)
        _gend_error_exit(gp, "genders_nodelist_create");

    if ((count = genders_query(gp, nodes, len, query)) < 0)
        _gend_error_exit(gp, query);

    /* Create a hostlist containing the list of nodes returned by the query */
    hl = hostlist_create(NULL);
    if (hl == NULL) {
        fprintf(stderr, "nodeattr: hostlist_create failed\n");
        exit(1);
    }
    for (i = 0; i < count; i++) {
        if (hostlist_push(hl, nodes[i]) == 0) {
            fprintf(stderr, "nodeattr: hostlist_push failed\n");
            exit(1);
        }
    }

    if (excludequery) {
        genders_nodelist_clear(gp, nodes);
      
        if ((count = genders_query(gp, nodes, len, excludequery)) < 0)
            _gend_error_exit(gp, excludequery);
    
        /* Do not check return code for == 0, node may not exist in hostlist */
        for (i = 0; i < count; i++)
            hostlist_delete(hl, nodes[i]);
    }

    genders_nodelist_destroy(gp, nodes);

    hostlist_sort(hl);
    str = _rangestr(hl, qfmt);
    if (strlen(str) > 0)
        printf("%s\n", str);
    free(str);
    hostlist_destroy(hl);
}

static int 
test_attr(genders_t gp, char *node, char *attr, int vopt)
{
    char *val = NULL;
    char *wantval;
    int res;

    if ((wantval = strchr(attr, '=')))  /* attr can actually be "attr=val" */
        *wantval++ ='\0';
   
    if (vopt || wantval)
        val = _val_create(gp); /* full of nulls initially */

    if ((res = genders_testattr(gp, node, attr, val, genders_getmaxvallen(gp) + 1)) < 0)
        _gend_error_exit(gp, "genders_testattr");

    if (vopt) {
        if (strlen(val) > 0)
            printf("%s\n", val);
    }
    if (wantval && strcmp(wantval, val) != 0)
        res = 0;
    if (vopt || wantval)
        free(val);
    return res;
}

static int 
test_query(genders_t gp, char *node, char *query)
{
    int res;

    if ((res = genders_testquery(gp, node, query)) < 0)
        _gend_error_exit(gp, "genders_testquery");

    return res;
}

static void
list_attr_val(genders_t gp, char *attr, int Uopt)
{
    char **nodes, **myvallist;
    char *val;
    int maxvallen, nlen, ncount, i, ret;
    unsigned int val_count = 0;
    
    /* achu: There is currently no library operation that offers
     * anything to easily access this information.  So we have to
     * iterate to do it.
     */

    if ((nlen = genders_nodelist_create(gp, &nodes)) < 0)
        _gend_error_exit(gp, "genders_getnodelist_create");

    if ((ncount = genders_getnodes(gp, nodes, nlen, attr, NULL)) < 0)
        _gend_error_exit(gp, "genders_getnodes");

    myvallist = (char **)_safe_malloc(ncount * sizeof(char **));
    for (i = 0; i < ncount; i++)
        myvallist[i] = _val_create(gp);

    val = _val_create(gp); /* full of nulls initially */

    if ((maxvallen = genders_getmaxvallen(gp)) < 0)
        _gend_error_exit(gp, "genders_getmaxvallen");

    for (i = 0; i < ncount; i++) {
        memset(val, '\0', maxvallen + 1);
        if ((ret = genders_testattr(gp, 
                                    nodes[i],
                                    attr, 
                                    val, 
                                    maxvallen + 1)) < 0)
            _gend_error_exit(gp, "genders_testattr");
        if (ret && strlen(val)) {
            int j, store = 0;
            if (Uopt) {
                /* achu: I know this is inefficient.  I don't have
                   good data structures around to make things
                   better/easier.
                 */
                for (j = 0; j < val_count; j++) {
                    if (!strcmp(val, myvallist[j])) {
                        store++;
                        break;
                    }
                }
            }
            if (!store) {
                strcpy(myvallist[val_count], val);
                val_count++;
            }
        }
    }
    
    for (i = 0; i < val_count; i++) {
        printf("%s\n", myvallist[i]);
    }

    genders_nodelist_destroy(gp, nodes);
    for (i = 0; i < ncount; i++)
        free(myvallist[i]);
    free(myvallist);
    free(val);
}

static void 
list_attrs(genders_t gp, char *node)
{
    char **attrs, **vals;
    int len, vlen, count, i;

    if ((len = genders_attrlist_create(gp, &attrs)) < 0)
        _gend_error_exit(gp, "genders_attrlist_create");
    if ((vlen = genders_vallist_create(gp, &vals)) < 0)
        _gend_error_exit(gp, "genders_vallist_create");
    if (node) {
        if ((count = genders_getattr(gp, attrs, vals, len, node)) < 0)
            _gend_error_exit(gp, "genders_getattr");
    } else {
        if ((count = genders_getattr_all(gp, attrs, len)) < 0)
            _gend_error_exit(gp, "genders_getattr_all");
    }
    for (i = 0; i < count; i++)
        if (node && strlen(vals[i]) > 0)
            printf("%s=%s\n", attrs[i], vals[i]);
        else
            printf("%s\n", attrs[i]);
    genders_attrlist_destroy(gp, attrs);
    genders_vallist_destroy(gp, vals);
}

static void 
usage(void)
{
    fprintf(stderr,
        "Usage: nodeattr [-f genders] [-q|-c|-n|-s] [-X exclude_query] query\n"
        "or     nodeattr [-f genders] [-q|-c|-n|-s] -A\n"
        "or     nodeattr [-f genders] [-v] [node] attr[=val]\n"
        "or     nodeattr [-f genders] -Q [node] query\n"
        "or     nodeattr [-f genders] -V [-U] attr\n"   
        "or     nodeattr [-f genders] -l [node]\n"
        "or     nodeattr [-f genders] -k\n"
        "or     nodeattr [-f genders] -d genders\n"    
            );
    exit(1);
}

static int
_diff(genders_t gh, genders_t dgh, char *filename, char *dfilename)
{
    char **nodes = NULL, **dnodes = NULL;
    int maxnodes, dmaxnodes, numnodes, dnumnodes;
    char **attrs = NULL, **dattrs = NULL;
    int maxattrs, dmaxattrs, numattrs, dnumattrs;
    char **vals = NULL, **dvals = NULL, *dvalbuf = NULL;
    int maxvals, dmaxvals, dmaxvallen;
    int i, j, rv, errcount = 0;

    /* Test #1: Determine if nodes match */

    if ((maxnodes = genders_nodelist_create(gh, &nodes)) < 0)
        _gend_error_exit(gh, "genders_nodelist_create");

    if ((numnodes = genders_getnodes(gh, nodes, maxnodes, NULL, NULL)) < 0)
        _gend_error_exit(gh, "genders_getnodes");

    if ((dmaxnodes = genders_nodelist_create(dgh, &dnodes)) < 0)
        _gend_error_exit(gh, "genders_nodelist_create");

    if ((dnumnodes = genders_getnodes(dgh, dnodes, dmaxnodes, NULL, NULL)) < 0)
        _gend_error_exit(dgh, "genders_getnodes");

    for (i = 0; i < numnodes; i++) {
        if ((rv = genders_isnode(dgh, nodes[i])) < 0)
            _gend_error_exit(dgh, "genders_isnode");

        if (!rv) {
            fprintf(stderr, "%s: Node \"%s\" does not exist\n", dfilename, nodes[i]);
            errcount++;
        }
    }

    for (i = 0; i < dnumnodes; i++) {
        if ((rv = genders_isnode(gh, dnodes[i])) < 0)
            _gend_error_exit(gh, "genders_isnode");

        if (!rv) {
            fprintf(stderr, "%s: Contains additional node \"%s\"\n", dfilename, dnodes[i]);
            errcount++;
        }
    }

    /* Test #2: Determine if attributes match */

    if ((maxattrs = genders_attrlist_create(gh, &attrs)) < 0)
        _gend_error_exit(gh, "genders_attrlist_create");

    if ((dmaxattrs = genders_attrlist_create(dgh, &dattrs)) < 0)
        _gend_error_exit(dgh, "genders_attrlist_create");

    if ((numattrs = genders_getattr_all(gh, attrs, maxattrs)) < 0)
        _gend_error_exit(gh, "genders_getattr_all");

    if ((dnumattrs = genders_getattr_all(dgh, dattrs, dmaxattrs)) < 0)
        _gend_error_exit(dgh, "genders_getattr_all");

    for (i = 0; i < numattrs; i++) {
        if ((rv = genders_isattr(dgh, attrs[i])) < 0)
            _gend_error_exit(dgh, "genders_isattr");

        if (!rv) {
            fprintf(stderr, "%s: Attribute \"%s\" does not exist\n", dfilename, attrs[i]);
            errcount++;
        }
    }

    for (i = 0; i < dnumattrs; i++) {
        if ((rv = genders_isattr(gh, dattrs[i])) < 0)
            _gend_error_exit(gh, "genders_isattr");

        if (!rv) {
            fprintf(stderr, "%s: Contains additional attribute \"%s\"\n", dfilename, dattrs[i]);
            errcount++;
        }
    }
    
    /* Test #3: For each node, are the attributes and values identical */

    if ((maxvals = genders_vallist_create(gh, &vals)) < 0)
        _gend_error_exit(gh, "genders_vallist_create");

    if ((dmaxvals = genders_vallist_create(dgh, &dvals)) < 0)
        _gend_error_exit(dgh, "genders_vallist_create");

    if ((dmaxvallen = genders_getmaxvallen(dgh)) < 0)
        _gend_error_exit(dgh, "genders_maxvallen");

    if (!(dvalbuf = malloc(dmaxvallen + 1))) {
        fprintf(stderr, "nodeattr: out of memory\n");
        exit(1);
    }

    for (i = 0; i < numnodes; i++) {

        /* Don't bother if the node doesn't exist, this issue has been
         * output already 
         */
        if ((rv = genders_isnode(dgh, nodes[i])) < 0)
            _gend_error_exit(dgh, "genders_isnode");

        if (!rv)
            continue;

        if (genders_attrlist_clear(gh, attrs) < 0)
            _gend_error_exit(gh, "genders_attrlist_clear");

        if (genders_vallist_clear(gh, vals) < 0)
            _gend_error_exit(gh, "genders_vallist_clear");

        if (genders_attrlist_clear(dgh, dattrs) < 0)
            _gend_error_exit(dgh, "genders_attrlist_clear");

        if (genders_vallist_clear(dgh, dvals) < 0)
            _gend_error_exit(dgh, "genders_vallist_clear");

        if ((numattrs = genders_getattr(gh, 
                                        attrs, 
                                        vals, 
                                        maxattrs, 
                                        nodes[i])) < 0)
          _gend_error_exit(gh, "genders_getattr");
        
        for (j = 0; j < numattrs; j++) {
          
            /* Don't bother if the attribute doesn't exist, this issue
             * has been output already
             */
            if ((rv = genders_isattr(dgh, attrs[j])) < 0)
                _gend_error_exit(dgh, "genders_isattr");

            if (!rv)
                continue;

            memset(dvalbuf, '\0', dmaxvallen + 1);
          
            if ((rv = genders_testattr(dgh, 
                                       nodes[i], 
                                       attrs[j], 
                                       dvalbuf, 
                                       dmaxvallen + 1)) < 0)
                _gend_error_exit(dgh, "genders_testattr");

            if (!rv) {
                fprintf(stderr, "%s: Node \"%s\" does not "
                        "contain attribute \"%s\"\n", 
                        dfilename, nodes[i], attrs[j]);
                errcount++;
                continue;
            }

            if (strlen(vals[j])) {
                if (strcmp(vals[j], dvalbuf)) {
                    if (strlen(dvalbuf)) {
                        fprintf(stderr, "%s: Node \"%s\", attribute \"%s\" has "
                                "a different value \"%s\"\n",
                                dfilename, nodes[i], attrs[j], dvalbuf);
                    }
                    else {
                        fprintf(stderr, "%s: Node \"%s\", attribute \"%s\" does "
                                "not have a value\n",
                                dfilename, nodes[i], attrs[j]);
                    }
                    errcount++;
                    continue;
                }
            }
            else {
                if (strlen(dvalbuf)) {
                    fprintf(stderr, "%s: Node \"%s\", attribute \"%s\" has "
                            "a value \"%s\"\n",
                            dfilename, nodes[i], attrs[j], dvalbuf);
                    errcount++;
                    continue;
                }
            }
        }

        /* There is no need to compare attribute values for the reverse
         * case.  Only for existence of attributes.
         */

        if ((dnumattrs = genders_getattr(dgh, 
                                         dattrs, 
                                         dvals, 
                                         dmaxattrs, 
                                         nodes[i])) < 0)
          _gend_error_exit(dgh, "genders_getattr");
        
        for (j = 0; j < dnumattrs; j++) {

            /* Don't bother if the attribute doesn't exist, this issue
             * has been output already
             */
            if ((rv = genders_isattr(gh, dattrs[j])) < 0)
                _gend_error_exit(dgh, "genders_isattr");

            if (!rv)
                continue;

            if ((rv = genders_testattr(gh, 
                                       nodes[i], 
                                       dattrs[j], 
                                       NULL,
                                       0)) < 0)
                _gend_error_exit(gh, "genders_testattr");
            
            if (!rv) {
                if (strlen(dvals[j])) {
                    fprintf(stderr, "%s: Node \"%s\" contains "
                            "an additional attribute value pair \"%s=%s\"\n", 
                            dfilename, nodes[i], dattrs[j], dvals[j]);
                }
                else {
                    fprintf(stderr, "%s: Node \"%s\" contains "
                            "an additional attribute \"%s\"\n", 
                            dfilename, nodes[i], dattrs[j]);
                }
                errcount++;
                continue;
            }
        }
    }

    (void)genders_nodelist_destroy(gh, nodes);
    (void)genders_nodelist_destroy(dgh, dnodes);
    (void)genders_attrlist_destroy(gh, attrs);
    (void)genders_attrlist_destroy(dgh, dattrs);
    (void)genders_vallist_destroy(gh, vals);
    (void)genders_vallist_destroy(dgh, dvals);
    free(dvalbuf);
    return errcount;
}

static void 
diff_genders(char *filename, char *dfilename)
{
    genders_t gh, dgh;

    gh = genders_handle_create();
    if (!gh) {
        fprintf(stderr, "nodeattr: out of memory\n");
        exit(1);
    }

    dgh = genders_handle_create();
    if (!dgh) {
        fprintf(stderr, "nodeattr: out of memory\n");
        exit(1);
    }
    
    if (genders_load_data(gh, filename) < 0)
        _gend_error_exit(gh, filename);
    
    if (genders_load_data(dgh, dfilename) < 0)
        _gend_error_exit(dgh, dfilename);

    if (_diff(gh, dgh, filename, dfilename) != 0)
        return;
}

/**
 ** Utility functions
 **/

static int 
_gend_error_exit(genders_t gp, char *msg)
{
    fprintf(stderr, "nodeattr: %s: %s\n", 
        msg, genders_strerror(genders_errnum(gp)));
    if (genders_errnum(gp) == GENDERS_ERR_PARSE) {
#if HAVE_GETOPT_LONG
        fprintf(stderr, "nodeattr: use --parse-check to debug errors\n");
#else
        fprintf(stderr, "nodeattr: use -k to debug errors\n");
#endif
    }
    exit(1);
}

static void *
_safe_malloc(size_t size)
{
    void *obj = (void *)malloc(size);

    if (obj == NULL) {
        fprintf(stderr, "nodeattr: out of memory\n");
        exit(1);
    }
    memset(obj, 0, size);
    return obj;
}

/* Create a host range string.  Caller must free result. */
static void *
_rangestr(hostlist_t hl, fmt_t qfmt)
{
    int size = 65536;
    char *str = _safe_malloc(size);

    /* FIXME: hostlist functions are supposed to return -1 on truncation.
     * This doesn't seem to be working, so make initial size big enough.
     */
    if (qfmt == FMT_HOSTLIST) {
        while (hostlist_ranged_string(hl, size, str) < 0) {
            free(str); 
            size += size;
            str = (char *)_safe_malloc(size);
        }
    } else {
        char sep = qfmt == FMT_SPACE ? ' ' : qfmt == FMT_COMMA ? ',' : '\n';
        char *p;

        while (hostlist_deranged_string(hl, size, str) < 0) {
            free(str); 
            size += size;
            str = (char *)_safe_malloc(size);
        }
        for (p = str; p != NULL; ) {
            if ((p = strchr(p, ',')))
                *p++ = sep;
        }
    }
    return str;
}

/* Create a value string.  Caller must free result. */
static char *
_val_create(genders_t gp)
{
    int maxvallen;
    char *val;

    if ((maxvallen = genders_getmaxvallen(gp)) < 0) 
        _gend_error_exit(gp, "genders_getmaxvallen");
    val = (char *)_safe_malloc(maxvallen + 1);

    return val;
}

#if 0
/* Create a node string.  Caller must free result. */
static char *
_node_create(genders_t gp)
{
    int maxnodelen;
    char *node;

    if ((maxnodelen = genders_getmaxnodelen(gp)) < 0) 
        _gend_error_exit(gp, "genders_getmaxnodelen");
    node = (char *)_safe_malloc(maxnodelen + 1);
    return node;
}

/* Create an attribute string.  Caller must free result. */
static char *
_attr_create(genders_t gp)
{
    int maxattrlen;
    char *attr;

    if ((maxattrlen = genders_getmaxattrlen(gp)) < 0) 
        _gend_error_exit(gp, "genders_getmaxattrlen");
    attr = (char *)_safe_malloc(maxattrlen + 1);
    return attr;
}

/* Convert "altname" to "gendname". Caller must free result. */
static char *
_to_gendname(genders_t gp, char *val)
{
    char **nodes;
    int count;
    int len;
    char *node = NULL;

    if ((len = genders_nodelist_create(gp, &nodes)) < 0) 
        _gend_error_exit(gp, "genders_nodelist_create");

    if ((count = genders_getnodes(gp, nodes, len, "altname", val)) < 0) {
        genders_nodelist_destroy(gp, nodes);
        _gend_error_exit(gp, val);
    } 
    if (count > 1)
        fprintf(stderr, "nodeattr: altname=%s appears more than once!\n", val);

    if (count == 1) {
        node = _node_create(gp);
        strcpy(node, nodes[0]);
    }
    genders_nodelist_destroy(gp, nodes);

    return node;
}
#endif

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
