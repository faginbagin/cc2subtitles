#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <getopt.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "PS_pack.h"
#include "PES_packet.h"

int stripivtv();

static const char* shortOpts = "i:o:l:v";

static struct option longOpts[] =
{
    { "in",     required_argument,  0,  'i' },
    { "out",    required_argument,  0,  'o' },
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
-o, --out out.mpg   Output MPEG2 file\n\
                    Default: stdout\n\
-l, --log logfile   Log output file\n\
                    Default: stderr\n\
-v, --verbose       Increment verbosity\n\
                    Default: 0\n\
");
}

int verbose = 0;
FILE* fpin = stdin;
FILE* fpout = stdout;
FILE* fplog = stderr;

int
main(int argc, char** argv)
{
    int c;
    int index;

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
            case 'o':
                if ((fpout = fopen(optarg, "w")) == NULL)
                {
                    fprintf(stderr, "Cannot open %s: %s\n", optarg, strerror(errno));
                    exit(1);
                }
                break;
            case 'l':
                if ((fplog = fopen(optarg, "w")) == NULL)
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

    return stripivtv();
}

int
stripivtv()
{
    PS_pack pack;
    PES_packet pes;
    int c = 0;

    while (c != EOF)
    {
        if (c == 0)
            c = decodeStartCode(fpin);

        if (c == MPEG_program_end_code || c == EOF)
        {
            break;
        }

        if (c == PS_pack_start_code)
        {
            pack.reset();
            pack.stream_id = c;
            c = pack.copy(fpin, fpout);
            if (c == EOF)
                break;
        }
        else if (c == private_stream_1)
        {
            // We need to remove the PS_pack that precedes thie PES_packet
            if (fseeko(fpout, pack.startPos, SEEK_SET) < 0)
            {
                fprintf(stderr, "stripivtv: seek failed: %s\n", strerror(errno));
                return EOF;
            }
            pes.reset();
            pes.stream_id = c;
            c = pes.decode(fpin, 0);
            if (c == EOF)
                break;
        }
        else if (c >= program_stream_map)
        {
            pes.reset();
            pes.stream_id = c;
            c = pes.copy(fpin, fpout);
            if (c == EOF)
                break;
        }
        else
        {
            // We must have an invalid stream id
            fprintf(stderr, "main: pos=%lld: Invalid stream_id=%#x\n",
                (long long)ftello(fpin)-4, c);
            c = 0;
        }
    }

    if (fclose(fpout) != 0)
    {
        fprintf(stderr, "Cannot close outfile: %s\n", strerror(errno));
        return EOF;
    }
    return 0;
}
