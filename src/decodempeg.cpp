#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "PS_pack.h"
#include "PES_packet.h"

int decode();

static char* shortOpts = "i:l:v";

static struct option longOpts[] =
{
    { "in",     required_argument,  0,  'i' },
    { "log",    required_argument,  0,  'l' },
    { "verbose",    no_argument,    0,  'v' },
};

void
usage(const char* prog)
{
    fprintf(stderr, "Usage: %s [option ...]\n", prog);
    fprintf(stderr, "Valid options are:\n\
-i, --in input.mpg  Input MPEG2 file\n\
                    Default: stdin\n\
-l, --log logfile   Log output file\n\
                    Default: stdout\n\
-v, --verbose       Increment verbosity\n\
                    Default: 0\n\
");
}

int verbose = 0;
FILE* fpin = stdin;
FILE* fplog = stdout;

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
            default:
                usage(argv[0]);
                exit(1);
                break;
        }
    }

    return decode();
}

int
decode()
{
    PS_pack pack;
    PES_packet pes;
    int c = 0;

    while (c != EOF)
    {
        if (c == 0)
            c = decodeStartCode(fpin);

        if (c == PS_pack_start_code)
        {
            pack.reset();
            pack.stream_id = c;
            c = pack.decode(fpin, 1);
            if (c == EOF)
                break;
            pack.print(fplog);
        }
        else if (c >= program_stream_map)
        {
            pes.reset();
            pes.stream_id = c;
            c = pes.decode(fpin, 1);
            if (c == EOF)
                break;
            pes.print(fplog);
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
