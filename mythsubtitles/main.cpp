#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "Decoder.h"

extern int decode();

static const char* shortOpts = "i:l:vap:P:x:f:";

static struct option longOpts[] =
{
    { "in",     required_argument,  0,  'i' },
    { "log",    required_argument,  0,  'l' },
    { "verbose",    no_argument,    0,  'v' },
    { "alternate",  no_argument,    0,  'a' },
    { "prefix", required_argument,  0,  'p' },
    { "page",   required_argument,  0,  'P' },
    { "xml",    required_argument,  0,  'x' },
    { "width",  required_argument,  0,  'w' },
    { "height", required_argument,  0,  'h' },
    { "fudge",  required_argument,  0,  'f' },
};

void
usage(const char* prog)
{
    fprintf(stderr, "Usage: %s [option ...]\n", prog);
    fprintf(stderr, "Valid options are:\n\
-i, --in input.mpg  Input media file containing closed captions\n\
                    Required\n\
-l, --log logfile   Log output file\n\
                    Default: stdout\n\
-v, --verbose       Increment verbosity\n\
                    Default: 0\n\
-a, --alternate     Alternate fields (needed for some dvd videos)\n\
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
-w, --width width   DVD output frame width (not input media width)\n\
                    Default: 720\n\
-h, --height height DVD output frame height (not input media height)\n\
                    Default: 480 for NTSC, 576 for PAL\n\
-f, --fudge factor  Multiply subtitle timestamps by this value\n\
                    For example, if transcoding changes video length from\n\
                    1 hour to 1 hour and 10 seconds, use 3610/3600 = 1.00277\n\
                    Default: 1.00293 for nuv recordings\n\
                             1.0 for all other recordings\n\
");
}

int verbose = 0;
int alternate = 0;
const char* infile = 0;
FILE* fplog = stdout;
const char* xmlfile = "spumux.xml";
FILE* fpxml;
const char* prefix = "";
int   pageno = 1;
// Frame width & height, if not set on command line, they'll be set
// when we get the first vbi page event in vbi.cpp
int   width = 0;
int   height = 0;
double fudgeFactor = (double)0.0;

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
                infile = strdup(optarg);
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
            case 'a':
                alternate++;
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
            case 'f':
                fudgeFactor = atof(optarg);
                break;
            default:
                usage(argv[0]);
                exit(1);
                break;
        }
    }

    // Make sure infile is defined
    if (!infile)
    {
        fprintf(stderr, "Required input file missing\n");
        usage(argv[0]);
        exit(1);
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
    Decoder* dec = Decoder::GetDecoder();
    if (!dec)
        return -1;
    int ret = dec->decode();
    delete dec;
    return ret;
}

