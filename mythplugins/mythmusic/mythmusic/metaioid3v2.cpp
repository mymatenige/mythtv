#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sys/stat.h>
using namespace std;

#include <mad.h>

#include "metaioid3v2.h"
#include "metadata.h"

#include "metaio_libid3hack.h"


#include <mythtv/mythcontext.h>

//==========================================================================
MetaIOID3v2::MetaIOID3v2(void)
    : MetaIO(".mp3")
{
}

//==========================================================================
MetaIOID3v2::~MetaIOID3v2(void)
{
}


//==========================================================================
/*!
 * \brief Writes metadata back to a file
 *
 * \param mdata A pointer to a Metadata object
 * \param exclusive Flag to indicate if only the data in mdata should be
 *                  in the file. If false, any unrecognised tags already
 *                  in the file will be maintained.
 * \returns Boolean to indicate success/failure.
 */
bool MetaIOID3v2::write(Metadata* mdata, bool exclusive)
{
    // Sanity check.
    if (!mdata)
        return false;

    // libid3tag hack....
    // Read the first 4 bytes of the file id3file. This is just a quick and
    // dirty way to determin the id3v2 tag version.
    // 49 44 33 03 Where the 03 denotes id3v2.3
    // When writing, libid3tag used 49 44 33 04 which is id3v2.4 which is not
    // as widely accepted. So, as we know we're not going to be doing anything
    // fancy, we will manually check this number. If it is 04, we leave well
    // alone, but if it's not, we will make it 03 for better compatibility.

    FILE* p_hack = fopen(mdata->Filename().local8Bit(), "rb");
    if (!p_hack)
        p_hack = fopen(mdata->Filename().ascii(), "rb");
    if (!p_hack)
        return false;

    unsigned char sig[4];
    bool bln_hack_version = false;
    
    if (4 == fread(&sig, 1, 4, p_hack)
        && 'I' == sig[0]
        && 'D' == sig[1]
        && '3' == sig[2]
        && sig[3] < 0x04)
    {
        bln_hack_version = true;
    }
    fclose(p_hack);
    
    id3_file* p_input = NULL;
    
    p_input = id3_file_open(mdata->Filename().local8Bit(), ID3_FILE_MODE_READWRITE);
    if (!p_input)
        p_input = id3_file_open(mdata->Filename().ascii(), ID3_FILE_MODE_READWRITE);
  
    if (!p_input)
      return false;

    // We don't like id3v1 tags... too limiting.
    // We don't write them on encoding, so we delete them if one exists.
    id3_tag *tag = id3_file_tag(p_input);
    if (!tag)
    {
        id3_file_close(p_input);
        return false;
    }

    if (exclusive)
    {
       id3_tag_clearframes(tag);
    }

    if (!mdata->Artist().isEmpty())
    {
        if (!exclusive) removeComment(tag, ID3_FRAME_ARTIST);
        setComment(tag, ID3_FRAME_ARTIST, mdata->Artist());
    }
    
    if (!mdata->Title().isEmpty())
    {
        if (!exclusive) removeComment(tag, ID3_FRAME_TITLE);
        setComment(tag, ID3_FRAME_TITLE, mdata->Title());
    }
    
    if (!mdata->Album().isEmpty())
    {
        if (!exclusive) removeComment(tag, ID3_FRAME_ALBUM);
        setComment(tag, ID3_FRAME_ALBUM, mdata->Album());
    }
    
    if (mdata->Year() > 999
        && mdata->Year() < 10000) // 4 digit year.
    {
        if (!exclusive) removeComment(tag, ID3_FRAME_YEAR);
        bln_hack_version = false; // v2.4 changes TYER to TDRC so need v2.4
        setComment(tag, ID3_FRAME_YEAR, QString("%1").arg(mdata->Year()));
    }

    QString data = mdata->Genre();
    if (!data.isEmpty())
    {
        if (!exclusive) removeComment(tag, ID3_FRAME_GENRE);
        
        id3_ucs4_t* p_ucs4 = id3_utf8_ucs4duplicate((const id3_utf8_t*)(const char*)data.utf8());
   
        int genrenum = id3_genre_number(p_ucs4);

        free(p_ucs4);

        if (genrenum >= 0)
            setComment(tag, ID3_FRAME_GENRE, QString("%1").arg(genrenum));
    }
    
    if (0 != mdata->Track())
    {
        if (!exclusive) removeComment(tag, ID3_FRAME_TRACK);
        setComment(tag, ID3_FRAME_TRACK, QString("%1").arg(mdata->Track()));
    }


    
    id3_tag_options(tag, ID3_TAG_OPTION_COMPRESSION, 0);
    id3_tag_options(tag, ID3_TAG_OPTION_CRC, 0);
    id3_tag_options(tag, ID3_TAG_OPTION_UNSYNCHRONISATION, 0);
    id3_tag_options(tag, ID3_TAG_OPTION_ID3V1, 0);
    
    // The id3_file_update function has a feature deficit.
    // See the files: metaio_libid3hack.c/h
    bool rv = (0 == ID3_FILE_UPDATE(p_input));

    rv = (0 == id3_file_close(p_input)) && rv;

    // Hack the tag version if necessary
    if (rv && bln_hack_version)
    {
        p_hack = fopen(mdata->Filename().local8Bit(), "rb+");
        if (!p_hack)
            p_hack = fopen(mdata->Filename().ascii(), "rb+");
    
        if (!p_hack)
            return rv; // This hack failing doesn't really denote a failure...
        
        rewind(p_hack);
        if (4 == fread(&sig, 1, 4, p_hack)
            && 'I' == sig[0]
            && 'D' == sig[1]
            && '3' == sig[2]
            && 0x04 == sig[3])
        {
            sig[3] = 0x03;
            rewind(p_hack);
            fwrite(&sig, 1, 4, p_hack);
        }
        fclose(p_hack); 
    }

    return rv;

    // Questions... Not sure about memory managment. I've not allocated any memory myself,
    //              but not sure of libid3tag's memory managment model.
}


//==========================================================================
/*!
 * \brief Reads Metadata from a file.
 *
 * \param filename The filename to read metadata from.
 * \returns Metadata pointer or NULL on error
 */
Metadata* MetaIOID3v2::read(QString filename)
{
    QString artist = "", album = "", title = "", genre = "";
    int year = 0, tracknum = 0, length = 0;
    id3_file *p_input = NULL;
    
    p_input = id3_file_open(filename.local8Bit(), ID3_FILE_MODE_READONLY);
    if (!p_input)
        p_input = id3_file_open(filename.ascii(), ID3_FILE_MODE_READONLY);

    if (p_input)
    {
        id3_tag *tag = id3_file_tag(p_input);
        
        if (!tag)
        {
            id3_file_close(p_input);
            return NULL;
        }
        
        title = getComment(tag, ID3_FRAME_TITLE);           
        artist = getComment(tag, ID3_FRAME_ARTIST);
        album = getComment(tag, ID3_FRAME_ALBUM);
        
        // Get Track Num dealing with 1/16, 2/16 etc. format
        tracknum = getComment(tag, ID3_FRAME_TRACK)
            .replace(QRegExp("^([0-9]*).*"), QString("\\1")).toInt();
        
        // NB Year could be TDRC or TYER depending on version....
        // From: http://www.id3.org/id3v2.4.0-structure.txt
        // Time stamps can be: yyyy, yyyy-MM, yyyy-MM-dd, yyyy-MM-ddTHH,
        //                     yyyy-MM-ddTHH:mm and yyyy-MM-ddTHH:mm:ss
        // Basically all starting yyyy.
        
        // Depending on the version of libid3tag, it will reassign a #define,
        // but we want to look for both.
        year = getComment(tag, "TDRC")
            .replace(QRegExp("^([0-9]{4}).*"), QString("\\1")).toInt();
        if (0 == year)
            year = getComment(tag, "TYER").toInt();
        
        // Genre.
        genre = getComment(tag, ID3_FRAME_GENRE);

        id3_ucs4_t *p_tmp = id3_utf8_ucs4duplicate((const id3_utf8_t*)(const char*)genre.utf8());
        
        id3_ucs4_t const *p_ucs4 = id3_genre_name(p_tmp);
        free(p_tmp);

        id3_utf8_t *p_utf8 = id3_ucs4_utf8duplicate(p_ucs4);  
        genre = (const char*)p_utf8;
        free(p_utf8);

        id3_file_close(p_input);
    }

    // Fallback to filename reading
    if (title.isEmpty())
    {
        readFromFilename(filename, artist, album, title, genre, tracknum);
    }

    length = getTrackLength(filename);

    Metadata *retdata = new Metadata(filename, artist, album, title, genre,
                                     year, tracknum, length);

    return retdata;
}


//==========================================================================
/*!
 * \brief Find the length of the track (in milliseconds)
 *
 * \param filename The filename for which we want to find the length.
 * \returns An integer (signed!) to represent the length in milliseconds.
 */
int MetaIOID3v2::getTrackLength(QString filename)
{
    struct mad_stream stream;
    struct mad_header header;
    mad_timer_t timer;

    unsigned char buffer[8192];
    unsigned int buflen = 0;

    mad_stream_init(&stream);
    mad_header_init(&header);
   
    timer = mad_timer_zero;

    FILE *input = fopen(filename.local8Bit(), "r");
    if (!input)
        input = fopen(filename.ascii(), "r");

    if (!input)
        return 0;

    struct stat s;
    fstat(fileno(input), &s);
    unsigned long old_bitrate = 0;
    bool vbr = false;
    int amount_checked = 0;
    int alt_length = 0;
    bool loop_de_doo = true;
    
    while (loop_de_doo) 
    {
        if (buflen < sizeof(buffer)) 
        {
            int bytes;
            bytes = fread(buffer + buflen, 1, sizeof(buffer) - buflen, input);
            if (bytes <= 0)
                break;
            buflen += bytes;
        }

        mad_stream_buffer(&stream, buffer, buflen);

        while (1)
        {
            if (mad_header_decode(&header, &stream) == -1)
            {
                if (!MAD_RECOVERABLE(stream.error))
                {
                    break;
                }
                if (stream.error == MAD_ERROR_LOSTSYNC)
                {
                    int tagsize = id3_tag_query(stream.this_frame,
                                                stream.bufend - 
                                                stream.this_frame);
                    if (tagsize > 0)
                    {
                        mad_stream_skip(&stream, tagsize);
                        s.st_size -= tagsize;
                    }
                }
            }
            else
            {
                if(amount_checked == 0)
                {
                    old_bitrate = header.bitrate;
                }
                else if(header.bitrate != old_bitrate)
                {
                    vbr = true;
                }
                if(amount_checked == 32 && !vbr)
                {
                    alt_length = (s.st_size * 8) / (old_bitrate / 1000);
                    loop_de_doo = false;
                    break;
                }
                amount_checked++;
                mad_timer_add(&timer, header.duration);
            }
            
        }
        
        if (stream.error != MAD_ERROR_BUFLEN)
            break;

        memmove(buffer, stream.next_frame, &buffer[buflen] - stream.next_frame);
        buflen -= stream.next_frame - &buffer[0];
    }

    mad_header_finish(&header);
    mad_stream_finish(&stream);

    fclose(input);

    if (vbr)
        return mad_timer_count(timer, MAD_UNITS_MILLISECONDS);

    return alt_length;
}

//==========================================================================
/*!
 * \brief Function to remove an individual comment in an ID3v2 Tag
 *
 * \param pTag Pointer to a id3_file object
 * \param pLabel The label of the comment you want to delete
 * \returns Nothing
 */
void MetaIOID3v2::removeComment(id3_tag *pTag,
                                const char* pLabel)
{
    if (!pLabel)
        return;
        
    struct id3_frame* p_frame = NULL;
        
    while ((p_frame = id3_tag_findframe(pTag, pLabel, 0)))
    {
        // Let's delete it!!
        if (0 == id3_tag_detachframe(pTag, p_frame))
            id3_frame_delete(p_frame);
    }
}


//==========================================================================
/*!
 * \brief Function to return an individual comment from an ID3v2 comment
 *
 * \param pTag Pointer to a id3_file object
 * \param pLabel The label of the comment you want
 * \returns QString containing the contents of the comment you want
 */
QString MetaIOID3v2::getComment(id3_tag *pTag, const char* pLabel)
{
    if (!pLabel)
        return "";
        
    struct id3_frame *p_frame = id3_tag_findframe(pTag, pLabel, 0);
    if (!p_frame)
        return "";
    
    id3_ucs4_t const *p_ucs4 = NULL;
    union id3_field *p_field;
    
    p_field = &p_frame->fields[1];
    unsigned int nstrings = id3_field_getnstrings(p_field);
    
    QString rv = "";
    for (unsigned int i=0; i<nstrings; ++i)
    {
        p_ucs4 = id3_field_getstrings(p_field, i);
        if (!p_ucs4)
        {
            break;
        }
        else
        {
            id3_utf8_t *p_utf8 = id3_ucs4_utf8duplicate(p_ucs4);
            if (!p_utf8)
                break;

            rv += QString::fromUtf8((const char*)p_utf8);

            free(p_utf8);
        }
    }

    return rv;
}

//==========================================================================
/*!
 * \brief Function to set an individual comment in an ID3v2 Tag
 *
 * \param pTag Pointer to a id3_file object
 * \param pLabel The label of the comment you want
 * \param rData A reference to the data you want to write
 * \returns Nothing
 */
bool MetaIOID3v2::setComment(id3_tag *pTag,
                             const char* pLabel, 
                             const QString& rData)
{
    if (!pLabel || "" == rData)
      return false;

    struct id3_frame* p_frame = NULL;
    id3_ucs4_t* p_ucs4 = NULL;

    p_frame = id3_frame_new(pLabel);

    if (NULL == p_frame)
      return false;

    // Test
    if (0 != id3_field_settextencoding(&p_frame->fields[0],
                                       ID3_FIELD_TEXTENCODING_UTF_16))
    {
        id3_frame_delete(p_frame);
        return false;
    }

    p_ucs4 = id3_utf8_ucs4duplicate((const id3_utf8_t*)(const char*)rData.utf8());

    if (!p_ucs4)
    {
        id3_frame_delete(p_frame);
        return false;      
    }

    if (0 != id3_field_setstrings(&p_frame->fields[1], 1, &p_ucs4))
    {
        free(p_ucs4);
        id3_frame_delete(p_frame);
        return false;      
    }

    free(p_ucs4);

    if (0 != id3_tag_attachframe(pTag, p_frame))
    {
        id3_frame_delete(p_frame);
        return false;      
    }

    return true;
}
