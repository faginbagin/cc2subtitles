
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "NuvDecoder.h"
#include "vbi.h"

// For some reason, after mythtranscode & ffmpeg are used to convert .nuv/mpeg4
// files into dvd compatible mpeg2 files, the mpeg2 files run a little over
// 10 seconds/hour longer than the original.
// Without this fudge factor, the subtitles will lag behind the audio and video.
// This number was picked from averaging the changes
// seen as a result of transcoding 4 different recordings.
#define FUDGE_FACTOR    (double)1.00293

NuvDecoder::NuvDecoder(AVFormatContext* f, int vStream)
    : Decoder(f)
{
    videoStream = vStream;
    avformat_close_input(&fmtCtx);
    fmtCtx = 0;
    fp = 0;
    buf = new unsigned char[1024];
    len = 1024;
    ccline = "";
    cccol = ccrow = 0;
    m_textlist = 0;
    if (fudgeFactor == 0)
        fudgeFactor = FUDGE_FACTOR;
    vbi->checkTime((int64_t)1);
}

NuvDecoder::~NuvDecoder()
{
    if (fp)
        fclose(fp);
    delete[] buf;
    delete m_textlist;
}

// Borrowed heavily from mythtv/libs/libmythtv/nuppeldecoder.cpp
int
NuvDecoder::decode()
{
    if ((fp = fopen(infile, "r")) == NULL)
    {
        fprintf(stderr, "%s: open failed: %s\n", infile, strerror(errno));
        return -errno;
    }

    long startPos = 0;
    int foundheader = 0;

    while (!foundheader && startPos < 20000)
    {
        // Read file header
        if (fread(&fileheader, sizeof(fileheader), 1, fp) != 1)
        {
            fprintf(stderr, "%s: Error reading file: %s\n",
                infile, strerror(errno));
            return EOF;
        }
        // Check for magic string
        if (strcmp(fileheader.finfo, "NuppelVideo") == 0 ||
            strcmp(fileheader.finfo, "MythTVVideo") == 0)
        {
            foundheader++;
            break;
        }
        // seek forward one byte and try again
        startPos++;
        if (fseek(fp, startPos, SEEK_SET) < 0)
        {
            fprintf(stderr, "%s: seek %ld failed: %s\n",
                infile, startPos, strerror(errno));
            return -errno;
        }
    }
    if (!foundheader)
    {
        fprintf(stderr, "%s: Bad file\n", infile);
        return EOF;
    }
    if (verbose)
        fprintf(fplog, "startPos=%ld\n", startPos);

    if (!readFrameHeader())
    {
        fprintf(stderr, "%s: File not big enough for a header\n", infile);
        return EOF;
    }

    if (frameheader.frametype != 'D')
    {
        fprintf(stderr, "%s: Illegal file format\n", infile);
        return EOF;
    }
    if (fseek(fp, (long)frameheader.packetlength, SEEK_CUR) < 0)
    {
        fprintf(stderr, "%s: File not big enough for first frame data\n",
            infile);
        return EOF;
    }

    if ((startPos = ftell(fp)) < 0)
    {
        fprintf(stderr, "%s: ftell failed: %s\n", infile, strerror(errno));
        return -errno;
    }
    if (!readFrameHeader())
        return EOF;

    // Do we need extradata?
    if (frameheader.frametype == 'X')
    {
        if (frameheader.packetlength != sizeof(extradata))
        {
            fprintf(stderr, "%s: Corrupt file.  Bad extended frame.\n",
                infile);
            return EOF;
        }
        if (fread(&extradata, sizeof(extradata), 1, fp) != 1)
            return EOF;

#if 0
        // We shouldn't need to jump around and read seek table or key frame table
        // We shouldn't need to search for apprpriate starting frame header
        if ((startPos = ftell(fp)) < 0)
        {
            fprintf(stderr, "%s: ftell failed: %s\n", infile, strerror(errno));
            return -errno;
        }
#endif
        if (!readFrameHeader())
            return EOF;
    }

#if 0
    // We shouldn't need to jump around and read seek table or key frame table
    // We shouldn't need to search for apprpriate starting frame header
    foundheader = 0;
    while (!foundheader && startPos < 20000)
    {
        switch (frameheader.frametype)
        {
            case 'A':
            case 'V':
            case 'S':
            case 'T':
            case 'R':
                foundheader++;
                break;
            default:
                // seek forward one byte and try again
                startPos++;
                if (fseek(fp, startPos, SEEK_SET) < 0)
                {
                    fprintf(stderr, "%s: seek %ld failed: %s\n",
                        infile, startPos, strerror(errno));
                    return -errno;
                }
                if (!readFrameHeader())
                    return EOF;
                break;
        }
    }
    if (!foundheader)
    {
        fprintf(stderr, "%s: Bad file\n", infile);
        return EOF;
    }
    if (fseek(fp, startPos, SEEK_SET) < 0)
    {
        fprintf(stderr, "%s: seek %ld failed: %s\n",
            infile, startPos, strerror(errno));
        return -errno;
    }
    fprintf(fplog, "startPos=%ld\n", startPos);
#endif

    while (isValidFrameType())
    {
        if (frameheader.frametype == 'T')
        {
            if (!processText())
                break;
        }
        else if (frameheader.frametype != 'R' && frameheader.packetlength != 0)
        {
            if (fseek(fp, (long)frameheader.packetlength, SEEK_CUR) < 0)
                break;
        }
        if (!readFrameHeader())
            break;
    }

    return 0;
}

int
NuvDecoder::readFrameHeader()
{
    if (fread(&frameheader, sizeof(frameheader), 1, fp) != 1)
        return 0;
    if (verbose > 2 || (verbose > 1 && frameheader.frametype == 'T'))
    {
        fprintf(fplog, "pos=%lld frametype=%c comptype=%c packetlength=%d timecode=%d\n",
            (long long)ftello(fp) - sizeof(frameheader),
            frameheader.frametype, frameheader.comptype,
            frameheader.packetlength, frameheader.timecode);
    }
    return 1;
}

int
NuvDecoder::isValidFrameType()
{
    switch (frameheader.frametype)
    {
        case 'A': case 'V': case 'S': case 'T': case 'R': case 'X':
        case 'M': case 'D': case 'Q': case 'K':
            return 1;
        default:
            fprintf(stderr, "%s pos=%lld: Invalid frametype=%c\n",
                infile, (long long)ftello(fp) - sizeof(frameheader),
                frameheader.frametype);
            return 0;
    }
}

int
NuvDecoder::processText()
{
    if (len < frameheader.packetlength);
    {
        delete[] buf;
        buf = new unsigned char[frameheader.packetlength];
        len = frameheader.packetlength;
    }
    if (fread(buf, frameheader.packetlength, 1, fp) != 1)
        return 0;

    switch (frameheader.comptype)
    {
        case 'C':
            return processCC();
        case 'T':
            return processTeletext();
        default:
            fprintf(stderr, "%s pos=%lld: Invalid comptype=%c for frametype=T\n",
                infile, (long long)ftello(fp) - sizeof(frameheader),
                frameheader.comptype);
            return 0;
    }
}

int
NuvDecoder::processCC()
{
    if (vbi->checkPageSize)
    {
        // let libzvbi initialize our page, then use it to initialize
        // subtitle dimensions in vbi.
        if (!vbi_fetch_cc_page(vbi->decoder, &page, 1, TRUE))
        {
            fprintf(stderr, "Can't init vbi_page\n");
            return 0;
        }
        vbi->setCaption(&page);
        // Use this as a reference blak page
        memcpy(&blankPage, &page, sizeof(page));
    }

    // Need to convert timecode from millisecs to 90KHz units
    vbi->checkTime((int64_t)frameheader.timecode * 90);

    // Copied from mythtv/libs/libmythtv/NuppelVideoPlayer.cpp
    // NuppelVideoPlayer::UpdateCC
    unsigned char* inpos = buf;

    struct ccsubtitle subtitle;
    memcpy(&subtitle, inpos, sizeof(subtitle));
    inpos += sizeof(subtitle);

    if (verbose > 1)
        fprintf(fplog, "ccsubtitle: row=%d rowcount=%d resumedirect=%#x resumetext=%#x clr=%d len=%d\n",
            subtitle.row, subtitle.rowcount, subtitle.resumedirect,
            subtitle.resumetext, subtitle.clr, subtitle.len);

    // skip undisplayed streams
    if ((subtitle.resumetext & CC_MODE_MASK) != CC_CC1)
        return 1;

    if (subtitle.row == 0)
        subtitle.row = 1;

    if (subtitle.clr)
    {
        //printf ("erase displayed memory\n");
        ResetCC();
        if (!subtitle.len)
            return 1;
    }

    if (verbose > 1 && subtitle.len)
    {
        logBytes(inpos, subtitle.len);
    }

//    if (subtitle.len || !subtitle.clr)
    {
        unsigned char *end = inpos + subtitle.len;
        int row = 0;
        int linecont = (subtitle.resumetext & CC_LINE_CONT);

        vector<ccText*> *ccbuf = new vector<ccText*>;
        vector<ccText*>::iterator ccp;
        ccText *tmpcc = NULL;
        int replace = linecont;
        int scroll = 0;
        bool scroll_prsv = false;
        int scroll_yoff = 0;
        int scroll_ymax = 15;

        do
        {
            if (linecont)
            {
                // append to last line; needs to be redrawn
                replace = 1;
                // backspace into existing line if needed
                int bscnt = 0;
                while ((inpos < end) && *inpos != 0 && (char)*inpos == '\b')
                {
                    bscnt++;
                    inpos++;
                }
                if (bscnt)
                    ccline.remove(ccline.length() - bscnt, bscnt);
            }
            else
            {
                // new line:  count spaces to calculate column position
                row++;
                cccol = 0;
                ccline = "";
                while ((inpos < end) && *inpos != 0 && (char)*inpos == ' ')
                {
                    inpos++;
                    cccol++;
                }
            }

            ccrow = subtitle.row;
            unsigned char *cur = inpos;

            //null terminate at EOL
            while (cur < end && *cur != '\n' && *cur != 0)
                cur++;
            *cur = 0;

            if (*inpos != 0 || linecont)
            {
                if (linecont)
                    ccline += QString::fromUtf8((const char *)inpos, -1);
                else
                    ccline = QString::fromUtf8((const char *)inpos, -1);
                tmpcc = new ccText();
                tmpcc->text = ccline;
                tmpcc->x = cccol;
                tmpcc->y = ccrow;
                tmpcc->color = 0;
                tmpcc->teletextmode = false;
                ccbuf->push_back(tmpcc);
            }
            subtitle.row++;
            inpos = cur + 1;
            linecont = 0;
        } while (inpos < end);

        // adjust row position
        if (subtitle.resumetext & CC_TXT_MASK)
        {
            // TXT mode
            // - can use entire 15 rows
            // - scroll up when reaching bottom
            if (ccrow > 15)
            {
                if (row)
                    scroll = ccrow - 15;
                if (tmpcc)
                    tmpcc->y = 15;
            }
        }
        else if (subtitle.rowcount == 0 || row > 1)
        {
            // multi-line text
            // - fix display of old (badly-encoded) files
            if (ccrow > 15)
            {
                ccp = ccbuf->begin();
                for (; ccp != ccbuf->end(); ccp++)
                {
                    tmpcc = *ccp;
                    tmpcc->y -= (ccrow - 15);
                }
            }
        }
        else
        {
            // scrolling text
            // - scroll up previous lines if adding new line
            // - if caption is at bottom, row address is for last
            // row
            // - if caption is at top, row address is for first row (?)
            if (subtitle.rowcount > 4)
                subtitle.rowcount = 4;
            if (ccrow < subtitle.rowcount)
            {
                ccrow = subtitle.rowcount;
                if (tmpcc)
                    tmpcc->y = ccrow;
            }
            if (row)
            {
                scroll = row;
                scroll_prsv = true;
                scroll_yoff = ccrow - subtitle.rowcount;
                scroll_ymax = ccrow;
            }
        }

        UpdateCCText(ccbuf, replace, scroll, scroll_prsv, scroll_yoff, scroll_ymax);
        delete ccbuf;
    }
    return 1;
}

int
NuvDecoder::processTeletext()
{
    if (vbi->checkPageSize)
    {
        // This attempt to initialize a page for Teletext doesn't work
        if (!vbi_fetch_vt_page(vbi->decoder, &page,
            0x888, 0, VBI_WST_LEVEL_3p5, 25, TRUE))
        {
            fprintf(stderr, "Can't init vbi_page\n");
            return 0;
        }
        vbi->setTeletext(&page);
    }

    // TODO
    return 1;
}

// Copied from mythtv/libs/libmythtv/osd.cpp
// OSD::UpdateCCText
int
NuvDecoder::UpdateCCText(vector<ccText*> *ccbuf,
                            int replace, int scroll, bool scroll_prsv,
                            int scroll_yoff, int scroll_ymax)
// ccbuf      :  new text
// replace    :  replace last lines
// scroll     :  scroll amount
// scroll_prsv:  preserve last lines and move into scroll window
// scroll_yoff:  yoff < scroll window <= ymax
// scroll_ymax:
{
    vector<ccText*>::iterator i;
    int visible = 0;

    if (m_textlist && (scroll || replace))
    {
        ccText *cc;

        // get last row
        int ylast = 0;
        i = m_textlist->end() - 1;
        cc = *i;
        if (cc)
            ylast = cc->y;

        // calculate row positions to delete, keep
        int ydel = scroll_yoff + scroll;
        int ykeep = scroll_ymax;
        int ymove = 0;
        if (scroll_prsv && ylast)
        {
            ymove = ylast - scroll_ymax;
            ydel += ymove;
            ykeep += ymove;
        }

        i = m_textlist->begin();
        while (i < m_textlist->end())
        {
            cc = (*i);
            if (!cc)
            {
                i = m_textlist->erase(i);
                continue;
            }

            if (cc->y > (ylast - replace))
            {
                // delete last lines
                delete cc;
                i = m_textlist->erase(i);
            }
            else if (scroll)
            {
                if (cc->y > ydel && cc->y <= ykeep)
                {
                    // scroll up
                    cc->y -= (scroll + ymove);
                    i++;
                }
                else
                {
                    // delete lines outside scroll window
                    i = m_textlist->erase(i);
                    delete cc;
                }
            }
            else
                i++;
        }
    }

    if (m_textlist)
        visible += m_textlist->size();

    if (ccbuf)
    {
        // add new text
        i = ccbuf->begin();
        while (i < ccbuf->end())
        {
            if (*i)
            {
                visible++;
                if (!m_textlist)
                    m_textlist = new vector<ccText *>;
                m_textlist->push_back(*i);
            }
            i++;
        }
    }

    if (m_textlist)
    {
        memcpy(&page, &blankPage, sizeof(page));
        ccText *cc;
        i = m_textlist->begin();
        for (; i != m_textlist->end(); i++)
        {
            cc = *i;
            if (verbose > 1)
                fprintf(fplog, "m_textlist: x=%d y=%d color=%d ttmode=%d text=%s\n",
                    cc->x, cc->y, cc->color, cc->teletextmode,
#if QT_VERSION < 0x040000
                    (const char*)cc->text.utf8());
#else
                    (const char*)cc->text.toUtf8());
#endif
            vbi_char* c = page.text + (cc->y-1)*page.columns + cc->x;
            const QChar* unicodeText = cc->text.unicode();
            int len = cc->text.length();
            for (; len > 0; len--,c++,unicodeText++)
            {
                c->unicode = unicodeText->unicode();
                c->opacity = VBI_OPAQUE;
            }
        }
        vbi->checkPage(&page);
    }

    return visible;
}

// Copied from mythtv/libs/libmythtv/NuppelVideoPlayer.cpp
// NuppelVideoPlayer::ResetCC
void NuvDecoder::ResetCC(void)
{
    ccline = "";
    cccol = 0;
    ccrow = 0;
    ClearAllCCText();
    vbi->checkPage(&blankPage);
}

// Copied from mythtv/libs/libmythtv/osdtypes.cpp
// OSDTypeCC::ClearAllCCText
void NuvDecoder::ClearAllCCText()
{
    if (m_textlist)
    {
        vector<ccText*>::iterator i = m_textlist->begin();
        for (; i != m_textlist->end(); i++)
        {
            ccText *cc = (*i);
            if (cc)
                delete cc;
        }
        delete m_textlist;
        m_textlist = NULL;
    }
}

