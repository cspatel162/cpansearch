#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include "cpans.h"
#include "membuf.h"
#include "utils.h"

int init( char * mirror_site ) {

    char url[256];
    membuf * mbuf;

    // check mirror_site url , should be end with "/"
    if( * (mirror_site + strlen(mirror_site) - 1 ) != '/' )
        strcat(mirror_site , "/");

    sprintf (url, "%s%s", mirror_site, "modules/02packages.details.txt.gz");

    fprintf( stderr, "Downloading source from %s\n" , url );
    mbuf = membuf_curl( url );

    char * tempfile = "packages.gz";
    assert( mbuf->buffer != NULL );
    membuf_writefile( mbuf , tempfile );
    membuf_free( mbuf );

    fprintf( stderr, "\nSource list saved.\n" );

    // use gunzip command to unzip the file..
    _gunzip( tempfile );
    unlink( tempfile );

    int len = strrchr( tempfile , '.' ) - tempfile;
    char outfile[32];
    strncpy( outfile , tempfile , len );
    *(outfile+len) = '\0';

    fprintf( stderr, "Transforming source list format.\n" );
    slist_transform( url , outfile );

    fprintf( stderr, "Done\n" );
    return 0;
}
