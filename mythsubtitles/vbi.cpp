
#include <string.h>
#include <stdio.h>
#include <sys/select.h>
#include <stdlib.h>
#include <unistd.h>

#include "vbi.h"

extern int verbose;
extern const char* infile;
extern FILE* fplog;
extern char* xmlfile;
extern FILE* fpxml;
extern char* prefix;
extern int pageno;  // Desired caption/teletext page
extern int width;   // Frame width
extern int height;  // Frame height
extern double fudgeFactor;

VBIDecoder::VBIDecoder()
{
    char* errstr = 0;
    firstTimestamp.base = 0;
    currTimestamp.base = 0;

    if (verbose > 1)
    {
        vbi_set_log_fn((vbi_log_mask)-1 /* log everything */, vbi_log_on_stderr, 0);
    }

    decoder = vbi_decoder_new();
    decoderTimestamp = 0.0;
    exporter = vbi_export_new("png", &errstr);
    if (!decoder || !exporter)
    {
        fflush(fplog);
        fprintf(stderr, "Cannot initialize libzvbi: %s\n",
           errstr);
        fflush(stderr);
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
    fprintf(fpxml, "<subpictures>\n<stream>\n<!-- %s -->\n", infile);

    checkPageSize = 1;

    memset(&lastPage, 0, sizeof(lastPage));
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
        fflush(fplog);
        fprintf(stderr, "No CC or Teletext data found\n");
        fflush(stderr);
        exit(1);
    }
    delete[] fileName;
    delete[] textBuf;
}

void
VBIDecoder::checkTime(int64_t ts)
{
    if (firstTimestamp.base == 0)
        firstTimestamp.base = ts;

    // Check for arithmetic wrap around.
    int64_t diff = ts - firstTimestamp.base;
    if (diff < 0)
        diff += 0x100000000LL;
    currTimestamp.base = diff;
}

void
VBIDecoder::decode(vbi_sliced* sliced, int nslice)
{
    // vbi_decode is very fussy about timestamps
    // if the difference between the previous time and the current time
    // is less than 0.025or greater than 0.050, it will blank the page
    // To keep it happy we give it timestamps that won't cause it
    // to ignore valid data just because the timestamps provided by
    // the ivtv driver don't quite match its expectations.
    decoderTimestamp += 0.03;
    fflush(fplog);
    vbi_decode(decoder, sliced, nslice, decoderTimestamp);
    fflush(stderr);
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
            checkPage(&page);
            vbi_unref_page(&page);
            break;
        case VBI_EVENT_CAPTION:
            if (event->ev.caption.pgno != pageno)
            {
                if (verbose)
                    fprintf(fplog, "vbi_event: type=CAPTION pgno=%d curr=%.4f ignored\n",
                        event->ev.caption.pgno, (double)currTimestamp);
                break;
            }
            if (!vbi_fetch_cc_page(decoder, &page, event->ev.caption.pgno, TRUE))
                break;
            if (verbose)
            {
                fprintf(fplog, "vbi_event: type=CAPTION pgno=%d curr=%.4f dirty: y=%d,%d roll=%d\n",
                    event->ev.caption.pgno, (double)currTimestamp,
                    page.dirty.y0, page.dirty.y1, page.dirty.roll);
                fflush(fplog);
            }
            if (checkPageSize)
                setCaption(&page);
            checkPage(&page);
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

void
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
                    fprintf(fplog, "Color change at %dx%d fg=%d bg=%d\n", col, row, fg, bg);
                }
            }
        }
    }

    if (nonEmpty == 0)
    {
        if (verbose)
            fprintf(fplog, "Blank page\n");
        flushPage(currTimestamp);
        memcpy(&lastPage, pg, sizeof(lastPage));
        return;
    }

    // If we haven't flushed the last page,
    // compare it against the current page
    if (fileName != 0)
    {
        if (memcmp(lastPage.text, pg->text, sizeof(lastPage.text)) == 0)
        {
            if (verbose)
                fprintf(fplog, "Identical pages\n");
            return;
        }
    }
    writePage(pg, currTimestamp);
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
        fflush(fplog);
        fprintf(stderr, "vbi_export_file(file=%s) failed: %s\n",
            fileName, vbi_export_errstr(exporter));
        fflush(stderr);
        exit(1);
    }

    // Copy the page so checkPage can compare it against future pages
    memcpy(&lastPage, pg, sizeof(lastPage));

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
            writeTimestamp(((double)startTimestamp) * fudgeFactor);
            fprintf(fpxml, " end=");
            writeTimestamp(((double)endTS) * fudgeFactor);
            fprintf(fpxml, " image=\"%s\"", fileName);
            fprintf(fpxml, " xoffset=\"%d\"", xOffset);
            fprintf(fpxml, " yoffset=\"%d\"", yOffset);
            fprintf(fpxml, ">\n<!-- original timestamps=%.4f,%.4f\n", (double)startTimestamp, (double)endTS);
            fwrite(textBuf, textBytes, 1, fpxml);
            fprintf(fpxml, "\n--></spu>\n");
            if (ferror(fpxml))
            {
                fflush(fplog);
                fprintf(stderr, "Error writing XML file\n");
                fflush(stderr);
                exit(1);
            }
            if (verbose)
                fflush(fpxml);
        }
        else if (verbose)
        {
            fprintf(fplog, "Ignoring file=%s start(%.4f) >= end(%.4f)\n",
                fileName, (double)startTimestamp, (double)endTS);
            fflush(fplog);
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
    // spumux doesn't require h:mm:ss.ff, so why bother?
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

    // Make sure yOffset is even
    if (yOffset & 1)
        yOffset++;

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

