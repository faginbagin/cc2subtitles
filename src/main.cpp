#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "PS_pack.h"
#include "PES_packet.h"
#include "vbi.h"

int decode();

static char* shortOpts = "i:l:vp:P:x:w:h:s:";

static struct option longOpts[] =
{
    { "in",     required_argument,  0,  'i' },
    { "log",    required_argument,  0,  'l' },
    { "verbose",    no_argument,    0,  'v' },
    { "prefix", required_argument,  0,  'p' },
    { "page",   required_argument,  0,  'P' },
    { "xml",    required_argument,  0,  'x' },
    { "width",  required_argument,  0,  'w' },
    { "height", required_argument,  0,  'h' },
    { "skip",   required_argument,  0,  's' },
};

void
usage(const char* prog)
{
    fprintf(stderr, "Usage: %s [option ...]\n", prog);
    fprintf(stderr, "Valid options are:\n\
-i, --in input.mpg  Input MPEG2 file containing ivtv vbi stream\n\
                    Default: stdin\n\
-l, --log logfile   Log output file\n\
                    Default: stdout\n\
-v, --verbose       Increment verbosity\n\
                    Default: 0\n\
-p, --prefix prefix Prefix to subtitle image file names\n\
                    If prefix is a directory, you must provide a trailing slash\n\
                    e.g. -p /tmp/\n\
                    Default: null string\n\
-P, --page pageno   A closed caption or teletext page number\n\
                    Valid NTSC page numbers are:\n\
                    1 - CC1, 2 - CC2, 3 - CC3, 4 - CC4,\n\
                    5 - Text1, 6 - Text2, 7 - Text3, 8 - Text4\n\
                    Default: 1 (CC1)\n\
-x, --xml spumux.xml    XML file created for input to spumux\n\
                    Default: spumux.xml\n\
-w, --width width   Frame width\n\
                    Default: 720\n\
-h, --height height Frame height\n\
                    Default: 480 for NTSC, 576 for PAL\n\
-s, --skip count    Skip the first count VBI blocks\n\
                    Default: 0\n\
");
}

int verbose = 0;
FILE* fpin = stdin;
FILE* fplog = stdout;
char* xmlfile = "spumux.xml";
FILE* fpxml;
char* prefix = "";
int   pageno = 1;
// Frame width & height, if not set on command line, they'll be set
// when we get the first vbi page event in vbi.cpp
int   width = 0;
int   height = 0;
int   skipVBI;

int
main(int argc, char** argv)
{
    int c;
    int index;
    char* logfile = 0;

    while ((c = getopt_long(argc, argv, shortOpts, longOpts, &index)) != -1)
    {
        switch (c)
        {
            case 'i':
                if ((fpin = fopen(optarg, "r")) == NULL)
                {
                    fprintf(stderr, "Cannot open %s: %s\n", optarg, strerror(errno));
                    exit(1);
                }
                break;
            case 'l':
                unlink(optarg);
                logfile = strdup(optarg);
                if ((fplog = fopen(optarg, "a")) == NULL)
                {
                    fprintf(stderr, "Cannot open %s: %s\n", optarg, strerror(errno));
                    exit(1);
                }
                break;
            case 'v':
                verbose++;
                break;
            case 'p':
                prefix = strdup(optarg);
                break;
            case 'P':
                pageno = atoi(optarg);
                break;
            case 'x':
                xmlfile = strdup(optarg);
                break;
            case 'w':
                width = atoi(optarg);
                break;
            case 'h':
                height = atoi(optarg);
                break;
            case 's':
                skipVBI = atoi(optarg);
                break;
            default:
                usage(argv[0]);
                exit(1);
                break;
        }
    }

    // Open spumux XML file
    if ((fpxml = fopen(xmlfile, "w")) == NULL)
    {
        fprintf(stderr, "Cannot open %s: %s\n", xmlfile, strerror(errno));
        exit(1);
    }

    if (logfile)
    {
        // Done because libzvbi writes stuff to stderr
        if (freopen(logfile, "a", stderr) == NULL)
        {
            fprintf(stderr, "Cannot open %s: %s\n", optarg, strerror(errno));
            exit(1);
        }
    }

    return decode();
}

int
decode()
{
    PS_pack pack;
    PES_packet pes;
    VBIDecoder vbi;
    int c = 0;

    while (c != EOF)
    {
        if (c == 0)
            c = decodeStartCode(fpin);

        if (c == PS_pack_start_code)
        {
            pack.reset();
            pack.stream_id = c;
            c = pack.decode(fpin);
            if (c == EOF)
                break;
            if (verbose > 2)
                pack.print(fplog);
        }
        else if (c >= program_stream_map)
        {
            pes.reset();
            pes.stream_id = c;
            c = pes.decode(fpin);
            if (c == EOF)
                break;
            if (verbose > 2 /* || pes.stream_id == private_stream_1 */ )
                pes.print(fplog);

            if (pes.stream_id == private_stream_1)
                vbi.decode(pes);
        }
        else if (c == MPEG_program_end_code || c == EOF)
        {
            break;
        }
        else
        {
            // We must have an invalid stream id
            fprintf(stderr, "main: pos=%lld: Invalid stream_id=%#x\n",
                ftello(fpin)-4, c);
            c = 0;
        }
    }

    return 0;
}
