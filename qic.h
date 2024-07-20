///
/// Copyright (C) 2024 Vitaly Chipounov
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
///
/// The above copyright notice and this permission notice shall be included in all
/// copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
/// SOFTWARE.
///

#ifndef _QIC_H_
#define _QIC_H_

#include <inttypes.h>

struct qic_vtbl_t {
    uint8_t tag[4]; // should be 'VTBL'
    uint32_t nseg;  // # of logical segments
    char desc[44];  // description
    uint32_t date;  // date and time created
    uint8_t flag;   // bitmap
    uint8_t seq;    // multi cartridge sequence #
    uint16_t rev_major;
    uint16_t rev_minor;
    uint8_t vres[14];    // reserved for vendor extensions
    uint32_t start, end; // physical QFA block numbers
    // In Win98 & ME subtract 3 from above for zero based SEGMENT index
    // to start first data segment and start first directory segment.
    uint8_t passwd[8];  // if not used, start with a 0 byte
    uint32_t dir_size;  // size of file set directory region in bytes
    uint64_t data_size; // total size of data region in bytes
    uint8_t os_ver[2];  // major and minor #
    uint8_t source_drive_label[16];
    uint8_t ldev; // logical dev file set originated from
    uint8_t res;  // should be 0
    uint8_t comp; // compression bitmap, 0 if not used
    uint8_t os_type;
    uint8_t res2[2]; // more reserved stuff
} __attribute__((packed));

static const char *VTBL_TAG = "VTBL";
static const char *MDID_TAG = "MDID";

static const size_t SEG_SZ = 29696; // MSBackUP wants data and dir segs to be multiple of this
// in compressed file each segment including catalog start with cseg_head

// Flag for a raw data segment.
static const uint16_t RAW_SEG = 0x8000;

struct cseg_head_t {
    // Cumlative uncompressed bytes at end this segment.
    uint64_t cumulative_size;
} __attribute__((packed));

struct cframe_head_t {
    // Physical bytes in this segment, offset to next header.
    // MSb indicates compressed segment when 0.
    uint16_t segment_size;
} __attribute__((packed));

struct ms_dir_fixed_t {
    uint16_t rec_len;  // only valid in dir set or Win95 Data region
    uint32_t ndx[2];   // thought this was quad word pointer to data? apparently not
                       // ndx[0] varies, ndx[1] = 0, was unknow[8]
                       // in data section always seems to be 0xffffffff
    uint16_t path_len; // @ 0xA  # path chars, exits in catalog and data section
                       // however path chars only present in data section
    uint16_t unknww1;
    uint8_t flag;
    uint16_t unknww2;
    uint32_t file_len;
    uint8_t unknwb1[20];
    uint8_t attrib;
    uint8_t unknwb2[3];
    uint32_t c_datetime; // created
    uint32_t unknwl1;
    uint32_t a_datetime; // accessed
    uint32_t unknwl2;
    uint32_t m_datetime; // modified, as shown in DOS
    uint32_t unknwl3;
    uint16_t nm_len; // length of the long variable length name
} __attribute__((packed));

struct ms_dir_fixed2_t {
    uint8_t unkwn1[13];
    uint32_t var1;
    uint32_t var2;
    uint16_t nm_len; // Length of 2nd, short variable length name.
} __attribute__((packed));

#define SUBDIR   0x1  // This is a directory entry, not a file.
#define EMPTYDIR 0x2  // This marks an empty sub-directory.
#define DIRLAST  0x8  // Last entry in this directory.
#define DIREND   0x30 // Last entry in entire volume directory.

#define DAT_SIG  0x33CC33CCL // Signature at start of Data Segment.
#define EDAT_SIG 0x66996699L // Signature before the start of the data file.

#endif