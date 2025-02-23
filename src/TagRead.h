/*------------------------------------------------------/
/ TagRead
/-------------------------------------------------------/
/ Copyright (c) 2020-2021, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/------------------------------------------------------*/

// ID3 Part was started from id3read:
// http://www.rohitab.com/discuss/topic/34514-id3-tag-checkerupdater/

#ifndef _TAGREAD_H_
#define _TAGREAD_H_

#include "fatfs/ff.h"

typedef struct _id31 {
    char header[3];
    char title[30];
    char artist[30];
    char album[30];
    char year[4];
    char comment[28];
    char zero;
    unsigned char tracknum;
    unsigned char genre;
} id31;

typedef struct _id32frame {
    char ID[4];
    unsigned char sizebytes[4];
    char flags[2];
    uint64_t pos; // position in the file
    size_t size;
    char* data;
    bool hasFullData;
    bool isUnsynced;
    struct _id32frame* next;
} id32frame;

typedef struct _id322frame {
    char ID[3];
    unsigned char sizebytes[3];
    uint64_t pos; // position in the file
    size_t size;
    char* data;
    bool hasFullData;
    bool isUnsynced;
    struct _id322frame* next;
} id322frame;

typedef struct _id32 {
    char version[2];
    char flags;
    size_t size;
    id32frame* firstframe;
} id32;

typedef struct _id32flat {
    size_t size;
    char* buffer;
} id32flat;

typedef enum _mp4_data_t {
    reserved = 0x00000000,
    UTF8 = 0x00000001,
    UTF16 = 0x00000002,
    JPEG = 0x0000000d,
    PNG = 0x0000000e
} mp4_data_t;

typedef struct _MP4_ilst_item {
    char type[4];
    mp4_data_t data_type;
    uint32_t data_size;
    uint64_t pos; // position in the file
    char *data_buf;
    bool hasFullData;
    struct _MP4_ilst_item* next;
} MP4_ilst_item;

typedef struct _MP4_ilst {
    MP4_ilst_item *first;
    MP4_ilst_item *last;
} MP4_ilst;

typedef enum _mime_t {
    non = 0,
    jpeg,
    png
} mime_t;

typedef enum _ptype_t {
    other = 0x00,
    icon32x32 = 0x01,
    icon = 0x02,
    front_cover = 0x03,
    back_cover = 0x04,
    leaflet = 0x05,
    media = 0x06,
    lead_artist = 0x07,
    artist = 0x08,
    conductor = 0x09,
    band = 0x0a,
    composer = 0x0b,
    lyrcist = 0x0c,
    location = 0x0d,
    recording = 0x0e,
    performance = 0x0f,
    capture = 0x10,
    fish = 0x11,
    illustration = 0x12,
    band_logo = 0x13,
    pub_logo = 0x14
} ptype_t;

const size_t frame_size_limit = 1024;
const size_t frame_start_bytes = 16;

class TagRead
{
public:
    TagRead();
    ~TagRead();
    int loadFile(const char *filename);
    int getUTF8Track(char *str, size_t size);
    int getUTF8Title(char *str, size_t size);
    int getUTF8Album(char *str, size_t size);
    int getUTF8Artist(char *str, size_t size);
    int getUTF8Year(char *str, size_t size);
    int getPictureCount();
    int getPicturePos(int idx, mime_t *mime, ptype_t *ptype, uint64_t *pos, size_t *size, bool *isUnsynced);

private:
    FIL fil;

    id31 *id3v1;
    id32 *id3v2;

    size_t getBESize3(unsigned char *buf);
    size_t getBESize4(unsigned char *buf);
    size_t getBESize4SyncSafe(unsigned char *buf);

    FRESULT f_read_unsync(FIL* fp, void* buff, UINT btr, UINT* br, bool unsync);

    int GetID3HeadersFull(FIL *infile, int testfail, id31** id31save, id32** id32save);
    id32* ID32Detect(FIL *infile);
    int GetID32UTF8(const char *id3v22, const char *id3v23, char *str, size_t size);
    int GetID3IDCount(const char *id3v22, const char *id3v23);
    void ID32Print(id32* id32header);
    void ID32Free(id32* id32header);
    int getID32Picture(int idx, mime_t *mime, ptype_t *ptype, uint64_t *pos, size_t *size, bool *isUnsynced);

    int ID31Detect(char* header, id31 **id31header);
    void ID31Print(id31* id31header);
    void ID31Free(id31* id31header);

    int getListChunk(FIL *fil);
    int findNextChunk(FIL *fil, uint32_t end_pos, char chunk_id[4], uint32_t *pos, uint32_t *size);

    MP4_ilst mp4_ilst;
    void clearMP4_ilst();
    int getMP4Box(FIL *fil);
    int findNextMP4Box(FIL *fil, uint32_t end_pos, char chunk_id[4], uint32_t *pos, uint32_t *size);
    int GetMP4BoxUTF8(const char *mp4_type, char *str, size_t size);
    int GetMP4TypeCount(const char *mp4_type);
    int getMP4Picture(int idx, mime_t *mime, ptype_t *ptype, uint64_t *pos, size_t *size);
};

#endif //_TAGREAD_H_