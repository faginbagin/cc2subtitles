#ifndef VBI_H
#define VBI_H

#include <stdio.h>
#include <sys/types.h>

#include "libzvbi.h"

typedef struct 
{
    int minrow;
    int maxrow;
    int mincol;
    int maxcol;
} PageRegion;

class Timestamp
{
    public:
        int64_t base;    // 33 bits  in 90 KHz units (27MHz/300)
        double toSeconds() { return ((double)base)/90000; }
        Timestamp& operator=(const Timestamp& ts) { base = ts.base; return *this; }
        Timestamp& operator=(int ts) { base = (unsigned long long)ts; return *this; }
        operator double() { return ((double)base)/90000; }
};

class VBIDecoder
{
    public:
        VBIDecoder();
        ~VBIDecoder();

        void            checkTime(int64_t ts);
        void            decode(vbi_sliced* sliced, int nslice);

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
