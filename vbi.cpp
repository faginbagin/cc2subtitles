
#include <string.h>
#include <stdio.h>
#include <sys/select.h>
#include <stdlib.h>
#include <unistd.h>

#include "PES_packet.h"
#include "vbi.h"

#define IVTV_SLICED_TYPE_TELETEXT       0x1     // Teletext (uses lines 6-22 for PAL)
#define IVTV_SLICED_TYPE_CC             0x4     // Closed Captions (line 21 NTSC)
#define IVTV_SLICED_TYPE_WSS            0x5     // Wide Screen Signal (line 23 PAL)
#define IVTV_SLICED_TYPE_VPS            0x7     // Video Programming System (PAL) (line 16)

extern int verbose;
extern FILE* fpin;
extern FILE* fplog;
extern char* xmlfile;
extern FILE* fpxml;
extern char* prefix;
extern int pageno;  // Desired caption/teletext page
extern int width;   // Frame width
extern int height;  // Frame height

typedef union
{
    unsigned long       words[2];
    unsigned long long  llword;
    fd_set              bits;
} ScanlineMask;

VBIDecoder::VBIDecoder()
{
    char* errstr = 0;
    firstTimestamp.base = 0;
    prevTimestamp.base = 0;
    currTimestamp.base = 0;
    decoder = vbi_decoder_new();
    exporter = vbi_export_new("png", &errstr);
    if (!decoder || !exporter)
    {
        fprintf(stderr, "Cannot initialize libzvbi: %s\n",
           errstr);
        exit(1);
    } 
    vbi_event_handler_add(decoder, -1, eventHandler, this);

    fileName = new char[strlen(prefix)+64];
    *fileName = 0;
    fileIndex = 0;
    vbi_page page;
    textSize = sizeof(page.text)/sizeof(vbi_char)*4;
    textBuf = new char[textSize];

    // Write the xml opening tags
    fprintf(fpxml, "<subpictures>\n<stream>\n");

    checkPageSize = 1;
}

VBIDecoder::~VBIDecoder()
{
    // If there's a subtitle image that hasn't been written to the XML file,
    // do it now.
    flushPage(currTimestamp);

    vbi_decoder_delete(decoder);
    vbi_export_delete(exporter);

    // Write the xml closing tags, and close the file
    fprintf(fpxml, "</stream>\n</subpictures>\n");
    fflush(fpxml);
    fclose(fpxml);

    if (fileIndex == 0)
    {
        // We didn't find any CC or Teletext data
        // Unlink the xml file, print an error message
        // and exit with non-zero exit code
        unlink(xmlfile);
        fprintf(stderr, "No CC or Teletext data found\n");
        exit(1);
    }
}

int
VBIDecoder::decode(PES_packet& pes)
{
    unsigned char* buf = pes.PES_packet_data;
    int len = pes.PES_packet_data_length;
    ScanlineMask mask;
    unsigned char* ptr = buf + 4; // assume we'll find magic cookie
    int i;
    vbi_sliced sliced_array[36];
    vbi_sliced* sliced = sliced_array;
    vbi_sliced* sliced_end;

    // Look for one of the magic cookies identifying this buffer
    // as VBI data captured by the ivtv driver.
    if (memcmp(buf, "itv0", 4) == 0)
    { 
        memcpy(&mask, ptr, 8);
        ptr += 8;
    }
    else if (memcmp(buf, "ITV0", 4) == 0)
    {
        mask.words[0] = 0xffffffff;
        mask.words[1] = 0xf;
    }
    else
        return 0;

    if (pes.PTS_DTS_flags)
    {
        if (firstTimestamp.base == 0)
            firstTimestamp = pes.PTS;
        long long diff = pes.PTS.base - firstTimestamp.base;
        if (diff < 0)
            diff += 0x100000000LL;
        currTimestamp.base = (unsigned long long) diff;
    }

    if (verbose > 1)
        fprintf(fplog, "VBI: PTS=%.3f len=%d mask=%#llx\n",
            (double)currTimestamp, len, mask.llword);


    for (i = 0; i < 36; i++)
    {
        if (FD_ISSET(i, &mask.bits))
        {
            // We'll adjust the value of line later when we know the type
            sliced->line = i;
            sliced++;
            FD_CLR(i, &mask.bits);
        }
        // Optimization, skip to next word when this word is zero
        if (i < 32 && mask.words[0] == 0)
            i = 31;
        if (i >= 32 && mask.words[1] == 0)
            break;
    }
    sliced_end = sliced;

    sliced = sliced_array;
    for (unsigned char* end = buf + len; ptr+43 <= end; ptr += 43, sliced++)
    {
        if (sliced == sliced_end)
        {
            if (verbose > 1)
                fprintf(fplog, "Have more scanlines than bits in mask!\n");
            break;
        }
        int type = *ptr;
        // If line is < 18, it must be a field 1 scanline, just add 6
        // Otherwise, it depends on ivtv type
        if (sliced->line < 18)
            sliced->line += 6;
        else
        {
            if (type == IVTV_SLICED_TYPE_CC) // assume NTSC 525 scanlines
            {
                sliced->line += (263-18+6);
            }
            else // assume PAL 625 scanlines
            {
                sliced->line += (313-18+6);
            }
        }
        // set sliced->id based on type
        switch (type)
        {
        // Teletext (uses lines 6-22 for PAL)
        case IVTV_SLICED_TYPE_TELETEXT:
            sliced->id = VBI_SLICED_TELETEXT_B;
            break;

        // Closed Captions (line 21 NTSC)
        case IVTV_SLICED_TYPE_CC:
            sliced->id = VBI_SLICED_CAPTION_525;
            break;

        // Wide Screen Signal (line 23 PAL)
        case IVTV_SLICED_TYPE_WSS:
            sliced->id = VBI_SLICED_WSS_625; // VBI_SLICED_WSS_CPR1204?
            break;

        // Video Programming System (PAL) (line 16)
        case IVTV_SLICED_TYPE_VPS:
            sliced->id = VBI_SLICED_VPS;
            break;
        }

        // Copy the ivtv data to the vbi_sliced structure
        memcpy(sliced->data, ptr+1, 42);

        if (verbose > 1)
        {
            fprintf(fplog, "   ivtv-type=%#x line=%d zvbi-id=%#x", type, sliced->line, sliced->id);
            unsigned char* p = ptr+1;
            if (verbose > 2)
            {
                int n = (type == IVTV_SLICED_TYPE_CC) ? 2 : 42;
                for (i = 0; i < n; i++, p++)
                {
                    fprintf(fplog, " %02x", *p);
                    int c = (*p) & 0x7f;
                    if (0x20 <= c && c <= 0x7f)
                        fprintf(fplog, "(%c)", c);
                }
            }
            putc('\n', fplog);
        }
    }
    if (verbose && sliced != sliced_end)
    {
        fprintf(fplog, "Have fewer scanlines than bits in mask!\n");
    }
    vbi_decode(decoder, sliced_array, sliced - sliced_array,
        (double)currTimestamp);
}


void
VBIDecoder::eventHandler(vbi_event* event, void* user_data)
{
    ((VBIDecoder*)user_data)->eventHandler(event);
}

void
VBIDecoder::eventHandler(vbi_event* event)
{
    vbi_page page;

    switch (event->type)
    {
        case VBI_EVENT_CLOSE:
            fprintf(fplog, "vbi_event: type=CLOSE\n");
            break;
        case VBI_EVENT_TTX_PAGE:
            // UNTESTED!
            fprintf(fplog, "vbi_event: type=TTX_PAGE pgno=%d subno=%d\n",
                event->ev.ttx_page.pgno, event->ev.ttx_page.subno);
            if (event->ev.ttx_page.pgno != pageno)
            {
                break;
            }
            if (!vbi_fetch_vt_page(decoder, &page,
                event->ev.ttx_page.pgno,
                event->ev.ttx_page.subno,
                VBI_WST_LEVEL_3p5, 25, TRUE))
                    break;
            if (checkPageSize)
                setTeletext(&page);
            if (checkPage(&page) != 0)
            {
                writePage(&page, prevTimestamp);
            }
            else
            {
                flushPage(currTimestamp);
            }
            prevTimestamp = currTimestamp;
            vbi_unref_page(&page);
            break;
        case VBI_EVENT_CAPTION:
            if (event->ev.caption.pgno != pageno)
            {
                if (verbose)
                    fprintf(fplog, "vbi_event: type=CAPTION pgno=%d curr=%.3f ignored\n",
                        event->ev.caption.pgno, (double)currTimestamp);
                break;
            }
            if (!vbi_fetch_cc_page(decoder, &page, event->ev.caption.pgno, TRUE))
                break;
            if (checkPageSize)
                setCaption(&page);
            if (checkPage(&page) != 0)
            {
                if (verbose)
                    fprintf(fplog, "vbi_event: type=CAPTION pgno=%d curr=%.3f dirty: y=%d,%d roll=%d\n",
                        event->ev.caption.pgno, (double)currTimestamp,
                        page.dirty.y0, page.dirty.y1, page.dirty.roll);
                writePage(&page, prevTimestamp);
            }
            else
            {
                flushPage(currTimestamp);
            }
            prevTimestamp = currTimestamp;
            vbi_unref_page(&page);
            break;
        case VBI_EVENT_NETWORK:
            fprintf(fplog, "vbi_event: type=NETWORK\n");
            break;
        case VBI_EVENT_TRIGGER:
            fprintf(fplog, "vbi_event: type=TRIGGER\n");
            break;
        case VBI_EVENT_ASPECT:
            fprintf(fplog, "vbi_event: type=ASPECT\n");
            break;
        case VBI_EVENT_PROG_INFO:
            fprintf(fplog, "vbi_event: type=PROG_INFO\n");
            break;
        case VBI_EVENT_NETWORK_ID:
            fprintf(fplog, "vbi_event: type=NETWORK_ID\n");
            break;
        default:
            fprintf(fplog, "vbi_event: type=%#x\n", event->type);
            break;
    }
}

int
VBIDecoder::checkPage(vbi_page* pg)
{
    // Check for blank page,
    // filling in region to non-blank bounds
    region.mincol = pg->columns;
    region.maxcol = 0;
    region.minrow = pg->rows;
    region.maxrow = 0;
    int nonEmpty = 0;
    int fg = 7;
    int bg = 0;

    vbi_char* text = pg->text;

    for (int row = 0; row < pg->rows; row++)
    {
        for (int col = 0; col < pg->columns; col++, text++)
        {
            if (text->opacity != VBI_TRANSPARENT_SPACE)
            {
                nonEmpty++;
                if (region.mincol > col)
                    region.mincol = col;
                if (region.maxcol < col)
                    region.maxcol = col;
                if (region.minrow > row)
                    region.minrow = row;
                if (region.maxrow < row)
                    region.maxrow = row;
                if (verbose && (text->foreground != fg || text->background != bg))
                {
                    fg = text->foreground;
                    bg = text->background;
                    fprintf(fplog, "%dx%d fg=%d bg=%d\n", col, row, fg, bg);
                }
            }
        }
    }

    return nonEmpty;
}

void
VBIDecoder::writePage(vbi_page* pg, Timestamp& startTS)
{
    if (*fileName != 0)
        flushPage(startTS);

    sprintf(fileName, "%ssub%04d.png", prefix, fileIndex++);
    startTimestamp = startTS;
    if (!vbi_export_file(exporter, fileName, pg))
    {
        fprintf(stderr, "vbi_export_file(file=%s) failed: %s\n",
            fileName, vbi_export_errstr(exporter));
        exit(1);
    }

    fflush(fplog);
    textBytes = vbi_print_page_region(pg, textBuf, textSize, "UTF-8",
        TRUE, TRUE,
        region.mincol,  region.minrow,
        region.maxcol - region.mincol+1,
        region.maxrow - region.minrow+1);
    fflush(stderr);

    if (verbose)
    {
        fwrite(textBuf, textBytes, 1, fplog);
        putc('\n', fplog);
    }

    // Check for characters that spumux won't parse
    // null chars will be changed to space
    // pairs of dashes will be changed to underscores
    for (char* ptr = textBuf; ptr < textBuf+textBytes; ptr++)
    {
        switch (*ptr)
        {
            case 0:
                *ptr = ' ';
                break;
            case '-':
                if (*(ptr+1) == '-')
                {
                    *ptr++ = '_';
                    *ptr = '_';
                }
                break;
        }
    }
}

void
VBIDecoder::flushPage(Timestamp& endTS)
{
    if (*fileName != 0)
    {
        if (startTimestamp.base < endTS.base)
        {
            fprintf(fpxml, "<spu start=");
            writeTimestamp((double)startTimestamp);
            fprintf(fpxml, " end=");
            writeTimestamp((double)endTS);
            fprintf(fpxml, " image=\"%s\"", fileName);
            fprintf(fpxml, " xoffset=\"%d\"", xOffset);
            fprintf(fpxml, " yoffset=\"%d\"", yOffset);
            fprintf(fpxml, ">\n<!--\n");
            fwrite(textBuf, textBytes, 1, fpxml);
            fprintf(fpxml, "\n--></spu>\n");
            if (ferror(fpxml))
            {
                fprintf(stderr, "Error writing XML file\n");
                exit(1);
            }
        }
        else
        {
            fprintf(fplog, "Ignoring file=%s start(%.4f) >= end(%.4f)\n",
                fileName, (double)startTimestamp, (double)endTS);
        }
        *fileName = 0;
    }
}

void
VBIDecoder::writeTimestamp(double timestamp)
{
/*
    int fraction = ((int)(timestamp * 10000)) % 10000;
    int seconds = (int)timestamp;
    int min = (seconds/60) % 60;
    int hours = seconds/(60*60);
    seconds = seconds % 60;
    fprintf(fpxml, "\"%d:%02d:%02d.%04d\"", hours, min, seconds, fraction);
*/
    fprintf(fpxml, "\"%.4f\"", timestamp);
}

void
VBIDecoder::setTeletext(vbi_page* pg)
{
    if (width == 0)
        width = 720;
    if (height == 0)
        height = 576;
    // According to libzvbi documentation,
    // Teletext character cell dimensions are 12x10 pixels
    int pageWidth = pg->columns * 12;
    int pageHeight = pg->rows * 10;
    xOffset = (width - pageWidth) / 2;
    yOffset = (height - pageHeight) / 2;
    checkPageSize = 0;
    fprintf(fplog, "Teletext frame=%dx%d page=%dx%x xOffset=%d yOffset=%d\n",
        width, height, pageWidth, pageHeight, xOffset, yOffset);
}

void
VBIDecoder::setCaption(vbi_page* pg)
{
    if (width == 0)
        width = 720;
    if (height == 0)
        height = 480;
    // According to libzvbi documentation,
    // Caption character cell dimensions are 16x26 pixels
    int pageWidth = pg->columns * 16;
    int pageHeight = pg->rows * 26;
    xOffset = (width - pageWidth) / 2;
    yOffset = (height - pageHeight) / 2;

    // Make sure yOffset is even
    if (yOffset & 1)
        yOffset++;

    checkPageSize = 0;
    fprintf(fplog, "Caption frame=%dx%d rows=%d cols=%d page=%dx%d xOffset=%d yOffset=%d\n",
        width, height, pg->rows, pg->columns, pageWidth, pageHeight, xOffset, yOffset);

}

