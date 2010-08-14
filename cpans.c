#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>

#include <stdarg.h>
#include <sys/types.h>
#include <regex.h>
#include <glib.h>
#include <ctype.h>

#include "membuf.h"
#include "cpans.h"

char version[] = "0.1";
char ignore_case = 0;

char * skipword( char * s2 )
{
    while( isgraph(*s2) )
        s2++;
    return s2;
}

char * skipspace( char * s2 ) 
{
    while( *s2 == ' ' )
        s2++;
    return s2;
}

void cpansearch_datafile(char * path )
{
    sprintf( path , "%s/.cpansearch.dat" , g_get_home_dir() );
}



void slist_transform( const char * url , const char * sourcefile )
{
    char datafile[128];
    sourcemeta_t smeta;
    cpansearch_datafile( datafile );

    FILE *in = fopen( sourcefile , "rb" );
    FILE *out = fopen( datafile , "wb+" );

    char buffer[300];
    int i;
    for (i = 0; i < 9; i++)
        fgets( buffer , 300 , in );   // skip 9 lines (header)

    memset( &smeta , 0 , sizeof( sourcemeta_t ));
    sprintf( smeta.uri , "%s" , url );
    fwrite( &smeta , sizeof(sourcemeta_t) , 1 , out );


    int cnt = 0;
    char * s1, *s2;
    while( !feof(in) ) {

        moduledata_t mdata;
        memset( buffer , 0 , 300 );
        memset( &mdata , 0 , sizeof(moduledata_t) );
        fgets( buffer , 300 , in );

        if( strlen(buffer) == 0 )
            break;

        s1 = buffer;
        s2 = buffer;
        s2 = skipword( s2 );
        if( s1 == s2 ) break;
        memcpy( mdata.name , s1 , (s2-s1));
        s2 = skipspace( s2 );

        s1 = s2;
        s2 = s2;
        s2 = skipword( s2 );
        if( s1 == s2 ) break;
        memcpy( mdata.version , s1 , (s2-s1) );
        if( strcmp(mdata.version ,"undef") == 0 )
            strcpy(mdata.version,"0");
        s2 = skipspace( s2 );

        s1 = s2;
        s2 = s1;
        s2 = skipword( s2 );
        if( s1 == s2 ) break;
        memcpy( mdata.path , s1 , (s2-s1) );

        // printf( "%s - %s - %s\n" , mdata.name , mdata.version , mdata.path );
        fwrite( &mdata , sizeof(moduledata_t) , 1 , out  );
        cnt++;
    }

    printf( "%d packages recorded.\n" , cnt );

    fclose(out);
    fclose(in);
    unlink( sourcefile );
}



char * slist_url()
{
    FILE * in;
    char datafile[128];

    cpansearch_datafile( datafile );
    in = fopen (datafile, "rb+");
    assert( in != NULL );


    sourcemeta_t smeta;
    fread( &smeta , sizeof(sourcemeta_t) , 1 , in );
    printf( "Source list from: %s\n" , smeta.uri );
    fclose( in );
    return strdup(smeta.uri);
}



void _gunzip( char * file )
{
    char cmd[32];
    sprintf( cmd , "gunzip -f %s" , file );
    system( cmd );
}

int init( char * mirror_site )
{
    char url[256];
    membuf * mbuf;

    // check mirror_site url , should be end with "/"
    if( * (mirror_site + strlen(mirror_site) - 1 ) != '/' )
        strcat(mirror_site , "/");

    sprintf (url, "%s%s", mirror_site, "modules/02packages.details.txt.gz");

    printf( "Downloading source from %s\n" , url );
    mbuf = membuf_curl( url );

    char * tempfile = "packages.gz";
    assert( mbuf->buffer != NULL );
    membuf_writefile( mbuf , tempfile );
    membuf_free( mbuf );

    printf( "\nSource list saved.\n" );

    // use gunzip command to unzip the file..
    _gunzip( tempfile );
    unlink( tempfile );

    int len = strrchr( tempfile , '.' ) - tempfile;
    char outfile[32];
    strncpy( outfile , tempfile , len );
    *(outfile+len) = '\0';

    printf( "Transforming source list format.\n" );
    slist_transform( url , outfile );

    printf( "Done\n" );
    return 0;
}



int update()
{
    char * url = slist_url();

    membuf * mbuf = membuf_curl( url );
    assert( mbuf->buffer != NULL );

    char * tempfile = "packages.gz";
    membuf_writefile( mbuf , tempfile );
    printf( "\nSource list saved.\n" );

    _gunzip( tempfile );
    unlink( tempfile );

    int len = strrchr( tempfile , '.' ) - tempfile;
    char outfile[32];
    strncpy( outfile , tempfile , len );
    *(outfile+len) = '\0';

    slist_transform( url , outfile );

    membuf_free( mbuf );
    free(url);
    return 0;
}




int search(const char * pattern)
{

    char datafile[128];
    regex_t reg;
    FILE * in;
    moduledata_t mdata;
    sourcemeta_t smeta;
    char * url;

    cpansearch_datafile( datafile );

    in = fopen (datafile, "rb+");
    assert( in != NULL );

    fread( &smeta , sizeof(sourcemeta_t) , 1 , in );
    url = smeta.uri;
    printf( "Source list from: %s\n" , url );

    int flag = REG_NOSUB | REG_EXTENDED;
    if( ignore_case )
        flag = flag | REG_ICASE;

    assert( regcomp( &reg , pattern , flag ) == 0 );

    regmatch_t matchlist[1];

    while( !feof(in) ) {
        memset( &mdata , 0 , sizeof(moduledata_t) );
        fread( &mdata , sizeof(moduledata_t) , 1 , in );

        if( regexec( &reg , mdata.name  , 1 , matchlist , 0 ) == 0 ) {
            printf( "%-40s - %s (%s)\n" , mdata.name , mdata.version , mdata.path );
        }
    }
    fclose(in);
    return 0;
}



void help()
{
    printf( "cpans usage:\n\n" );
    printf( "  To init source list:\n\n" );
    printf( "     cpans -i http://cpan.nctu.edu.tw/ \n" );
    printf( "     cpans --init http://cpan.nctu.edu.tw/ \n\n" );

    printf( "  To update source list:\n\n" );
    printf( "     cpans --update  \n" );
    printf( "     cpans --u  \n\n" );

    printf( "  To search:\n\n" );
    printf( "     cpans [pattern]  \n\n" );
}


void init_local( char * localpath )
{



}


void init_from_minicpanrc()
{
    char minicpanrc[64];
    sprintf( minicpanrc , "%s/.minicpanrc" , g_get_home_dir() );

    if (!g_file_test (minicpanrc, G_FILE_TEST_EXISTS))
        return;



    FILE *rc = fopen( minicpanrc , "r" );
    char buffer[50] = {0};
    char localpath[64] = {0};

    strcat( localpath , "file://" );

    while(!feof(rc)){
        fgets( buffer , 50 , rc );
        if( strstr(buffer,"local:") == buffer ) {
            char * c = strchr(buffer,'/');
            strcat( localpath , c );

            // chomp
            *(localpath + strlen(localpath) - 1) = '\0';
            break;
        }
    }
    fclose(rc);
    assert( localpath != NULL );

    if( * (localpath + strlen(localpath) - 1 ) != '/' )
        strcat(localpath , "/");

    printf( "Found minicpanrc: %s\n" , localpath );
    init( localpath );
}

/* getopt setting start */
static int option_index = 0;
static int thisopt = 0;

static struct option long_options[] = {
  { "fetch"     , required_argument, 0 , 'f' },
  { "init"      , required_argument, 0 , 'f' },
  { "search"    , required_argument, 0 , 's' },
  { "update"    , required_argument, 0 , 'u' },
  { "recent"    , no_argument      , 0 , 'r' },
  { "help"      , no_argument      , 0 , 'h' },
};
/* getopt setting end */


int main(int argc, char **argv)
{
    setvbuf( stderr , 0, _IONBF, 0);
    setvbuf( stdout , 0, _IONBF, 0);

    while( (thisopt = getopt_long(argc, argv, "if:s:u:rh", long_options, &option_index)) != -1 ) {

      switch (thisopt) {
        case 0:
          break;

        case 'f':
          if (optarg != NULL) {
            init( (char*)optarg);
          } else {
            init_from_minicpanrc();
          }

        case 'i':
          ignore_case = 1;
          break;

        case 's':
          search(optarg);
          break;

        case 'u':
          puts("Update package list from mirror");
          update();
          break;

        case 'r':
          puts("Searching packages from recent index");
          // update package list
          break;

        case 'h':
          help();
          break;

      }

    }

    return 0;
}
