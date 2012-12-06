#ifndef VBI_H
#define VBI_H

#include <stdio.h>
#include <sys/types.h>

#include "libzvbi.h"

class PES_packet;

typedef struct 
{
    int minrow;
    int maxrow;
    int mincol;
    int maxcol;
} PageRegion;

class VBIDecoder
{
    public:
        VBIDecoder();
        ~VBIDecoder();

        void            checkTime(PES_packet& pes);
        void            decode(PES_packet& pes);

        static void     eventHandler(vbi_event* event, void* user_data);
        void            eventHandler(vbi_event* event);

        void            checkPage(vbi_page* pg);
        void            writePage(vbi_page* pg, Timestamp& startTS);
        void            flushPage(Timestamp& endTS);
        void            writeTimestamp(double timestamp);
        void            setTeletext(vbi_page* pg);
        void            setCaption(vbi_page* pg);

        Timestamp       firstTimestamp;
        Timestamp       currTimestamp;

        vbi_decoder*    decoder;
        double          decoderTimestamp;

        vbi_export*     exporter;
        PageRegion      region;

        char*           fileName;
        int             fileIndex;
        Timestamp       startTimestamp;
        char*           textBuf;
        int             textSize;
        int             textBytes;
        vbi_page        lastPage;

        int             checkPageSize;
        int             xOffset;
        int             yOffset;
        
};

#endif /* VBI_H */
