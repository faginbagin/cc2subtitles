#ifndef NUVDECODER_H
#define NUVDECODER_H

#include <stdio.h>
#include <qstring.h>
#include "libzvbi.h"

#include "format.h"
#include "osdtypes.h"

#include "Decoder.h"

class NuvDecoder : public Decoder
{
    public:
        NuvDecoder(AVFormatContext* f, int vStream);
        virtual ~NuvDecoder();

        virtual int decode();

    private:
        int videoStream;
        FILE* fp;
        struct rtfileheader fileheader;
        struct rtframeheader frameheader;
        struct extendeddata extradata;
        int readFrameHeader();
        int isValidFrameType();

        unsigned char* buf;
        int len;
        vbi_page page;
        vbi_page blankPage;
        QString ccline;
        int cccol;
        int ccrow;

        int processText();
        int processCC();
        int processTeletext();

        vector<ccText *> *m_textlist;
        int UpdateCCText(vector<ccText*> *ccbuf,
                            int replace, int scroll, bool scroll_prsv,
                            int scroll_yoff, int scroll_ymax);
        void ResetCC(void);
        void ClearAllCCText();
};

#endif /* NUVDECODER_H */
