/*
 * ts.c: MPEG TS functions for replex
 *        
 *
 * Copyright (C) 2003 Marcus Metzler <mocm@metzlerbros.de>
 *                    Metzler Brothers Systementwicklung GbR
 * Changes to use MythTV logging
 * Copyright (C) 2011 Gavin Hurlbut <ghurlbut@mythtv.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * General Public License for more details.
 *
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 * Or, point your browser to http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

#include "ts.h"
#include "element.h"
#include "pes.h"

#include "libmythbase/mythlogging.h"

uint16_t get_pid(const uint8_t *pid)
{
	uint16_t pp = 0;

	pp = (pid[0] & PID_MASK_HI)<<8;
	pp |= pid[1];

	return pp;
}

int find_pids_pos(uint16_t *vpid, uint16_t *apid, uint16_t *ac3pid,uint8_t *buf, int len, int *vpos, int *apos, int *ac3pos)
{
	int c=0;
	int found=0;

	if (!vpid || !apid || !ac3pid || !buf || !len) return 0;

	*vpid = 0;
	*apid = 0;
	*ac3pid = 0;

	while ( c+TS_SIZE < len){
		if (buf[c] == buf[c+TS_SIZE]) break;
		c++;
	}

	while(found<2 && c < len){
		if (buf[c+1] & PAY_START) {
			int off = 4;
			
			if ( buf[c+3] & ADAPT_FIELD) {  // adaptation field?
				off += buf[c+4] + 1;
			}
			
			if (off < TS_SIZE-4){
				if (!*vpid && (buf[c+off+3] & 0xF0) == 0xE0){
					*vpid = get_pid(buf+c+1);
					if (vpos) *vpos = c+off+3;
					found++;
				}
				if (!*ac3pid && buf[c+off+3] == 0xBD){
					int l=off+4;
					int f=0;

					while ( l < TS_SIZE && f<2){
						uint8_t b=buf[c+l];
						switch(f){
						case 0:
							if ( b == 0x0b) 
								f = 1;
							break;
							
						case 1:
							if ( b == 0x77)
								f = 2;
							else if ( b != 0x0b) 
								f = 0;
						}
						l++;
					}	
					if (f==2){
						*ac3pid = get_pid(buf+c+1);
						if (ac3pos) *ac3pos = c+off+3;
						found++;
					}
				}
				if (!*apid && ((buf[c+off+3] & 0xF0) == 0xC0 ||
					       (buf[c+off+3] & 0xF0) == 0xD0)){
					*apid = get_pid(buf+c+1);
					if (apos) *apos = c+off+3;
					found++;
				}
			}
		} 
		c+= TS_SIZE;
	}
	return found;
}


int find_pids(uint16_t *vpid, uint16_t *apid, uint16_t *ac3pid,uint8_t *buf, int len)
{
	return find_pids_pos(vpid, apid, ac3pid, buf, len, nullptr, nullptr, nullptr);
}

//taken and adapted from libdtv, (c) Rolf Hakenes
// CRC32 lookup table for polynomial 0x04c11db7
static const std::array <const uint32_t,256> crc_table {
   0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
   0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
   0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
   0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
   0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
   0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
   0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
   0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
   0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
   0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
   0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
   0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
   0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
   0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
   0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
   0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
   0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
   0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
   0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
   0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
   0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
   0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
   0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
   0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
   0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
   0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
   0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
   0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
   0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
   0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
   0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
   0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
   0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
   0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
   0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
   0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
   0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
   0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
   0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
   0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
   0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
   0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
   0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4};

static unsigned int crc32_04c11db7 (const unsigned char *d, int len, unsigned int crc)
{
   const unsigned char *u = d; // Saves '& 0xff'

   for (int i=0; i<len; i++)
      crc = (crc << 8) ^ crc_table[((crc >> 24) ^ *u++)];

   return crc;
}

static int write_ts_header(uint16_t pid, int payload_start, int count,
		    int64_t SCR, uint8_t *obuf, int stuff)
{
	int c = 0;

	obuf[c++] = 0x47;
	obuf[c++] = (payload_start ? 0x40 : 0x00) | ((pid >> 8) & 0x1f);
	obuf[c++] = pid & 0xff;
	obuf[c++] = ((SCR >= 0 || stuff) ? 0x30 : 0x10) | count;
	if (SCR >= 0|| stuff) {
		if (stuff)
			stuff--;
		int size = stuff;
		unsigned char flags = 0;
		if(SCR >= 0) {
			size = std::max(size, 7);
			flags |= 0x10;
		}
		obuf[c++] = size;
		if(size) {
			obuf[c++] = flags;
			size--;
		}
		if(SCR >= 0) {
			auto lscr = (uint32_t) ((SCR/300ULL) & 0xFFFFFFFFULL);
			uint8_t bit = (lscr & 0x01) << 7;
			lscr = htonl(lscr >> 1);
			auto *scr = (uint8_t *) &lscr;
			auto scr_ext = (uint16_t) ((SCR%300ULL) & 0x1FFULL);
			obuf[c++] = scr[0];
			obuf[c++] = scr[1];
			obuf[c++] = scr[2];
			obuf[c++] = scr[3];
			obuf[c++] = bit | 0x7e | (scr_ext >> 8);
			obuf[c++] = scr_ext & 0xff;
			size -= 6;
		}
		while(size-- > 0)
			obuf[c++] = 0xff;
	}
	return c;
}

int write_video_ts(uint64_t vpts, uint64_t vdts, uint64_t SCR, uint8_t *buf,
		   int *vlength, uint8_t ptsdts, ringbuffer *vrbuffer)
{
//Unlike program streams, we only do one PES per frame
	static int s_count = 0;
	int pos = 0;
	int stuff = 0;
	int length = *vlength;

	if (! length) return 0;
	int p = 4;
	if ( ptsdts ) {
		p += PES_H_MIN + 8;

		if ( ptsdts == PTS_ONLY) {
			p += 5;
		} else if (ptsdts == PTS_DTS){
			p += 10;
		}
	}
	if ( length+p >= TS_SIZE){
		length = TS_SIZE;
	} else {
		stuff = TS_SIZE - length - p;
		length = TS_SIZE;
	}
	if(ptsdts) {
#if 0
	LOG(VB_GENERAL, LOG_INFO, QString("SCR: %1 PTS: %2 DTS: %3")
	    .arg(SCR  / 27000000.0, 0,'f')
	    .arg(vpts / 27000000.0, 0,'f')
	    .arg(vdts / 27000000.0, 0,'f'));
#endif
		pos = write_ts_header(TS_VIDPID, 1, s_count, SCR, buf, stuff);
		// always use length == 0 for video streams
		pos += write_pes_header( 0xE0, 6, vpts, vdts, buf+pos, 
					 0, ptsdts);
	} else {
		pos = write_ts_header(TS_VIDPID, 0, s_count, -1, buf, stuff);
	}
	s_count = (s_count+1) & 0x0f;

	if (length-pos > *vlength){
                LOG(VB_GENERAL, LOG_ERR, QString("WHAT THE HELL  %1 > %2\n")
                    .arg(length-pos).arg(*vlength));
	}

	int add = ring_read( vrbuffer, buf+pos, length-pos);
	*vlength = add;
	if (add < 0) return -1;
	pos += add;

	return pos;
}

int write_audio_ts(int n, uint64_t pts, uint8_t *buf, int *alength,
		   uint8_t ptsdts, ringbuffer *arbuffer)
{
	static std::array<int,32> s_count {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
					   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int pos = 0;
	int stuff = 0;
	int length = *alength;

	if (!length) return 0;
	int p = 4;

	if (ptsdts == PTS_ONLY){
		p += PES_H_MIN + 5;
	}

	if ( length+p >= TS_SIZE){
		length = TS_SIZE;
	} else {
		stuff = TS_SIZE - length - p;
		length = TS_SIZE;
	}
	if(ptsdts) {
		pos = write_ts_header(TS_MP2PID+n, 1, s_count[n], -1, buf, stuff);
		pos += write_pes_header( 0xC0+n, *alength + PES_H_MIN + 5, pts,
					 0, buf+pos, 0, ptsdts);
	} else {
		pos = write_ts_header(TS_MP2PID+n, 0, s_count[n], -1, buf, stuff);
	}
	s_count[n] = (s_count[n]+1) & 0x0f;
	int add = ring_read( arbuffer, buf+pos, length-pos);
	*alength = add;
	if (add < 0) return -1;
	pos += add;

	if (pos != TS_SIZE) {
                LOG(VB_GENERAL, LOG_ERR, QString("apos: %1").arg(pos));
		exit(1);
	}

	return pos;
}

int write_ac3_ts(int n, uint64_t pts, uint8_t *buf, int *alength,
	 uint8_t ptsdts, int nframes, ringbuffer *ac3rbuffer)
{
	static std::array<int,32> s_count {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
					   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int pos = 0;
	int stuff = 0;
	int length = *alength;

	if (!length) return 0;
	int p = 4;

	if (ptsdts == PTS_ONLY){
		p += PES_H_MIN + 5 + 4; //PES header + PTS + PS1
	}

	if ( length+p >= TS_SIZE){
		length = TS_SIZE;
	} else {
		stuff = TS_SIZE - length - p;
		length = TS_SIZE;
	}
	if(ptsdts) {
		pos = write_ts_header(TS_AC3PID+n, 1, s_count[n], -1, buf, stuff);
		pos += write_pes_header( PRIVATE_STREAM1,
					 *alength + 4 + PES_H_MIN + 5,
					 pts, 0, buf+pos, 0, ptsdts);
		buf[pos] = 0x80 + n;
		buf[pos+1] = nframes;
		buf[pos+2] = 0x00;
		buf[pos+3] = 0x00;
		pos += 4;
	} else {
		pos = write_ts_header(TS_AC3PID+n, 0, s_count[n], -1, buf, stuff);
	}
	s_count[n] = (s_count[n]+1) & 0x0f;

	int add = ring_read( ac3rbuffer, buf+pos, length-pos);
	*alength = add;
	if (add < 0) return -1;
	pos += add;

	if (pos != TS_SIZE) {
                LOG(VB_GENERAL, LOG_ERR, QString("apos: %1").arg(pos));
		exit(1);
	}

	return pos;
}

void write_ts_patpmt(extdata_t *ext, int extcnt, uint8_t prog_num, uint8_t *buf)
{
	static constexpr uint8_t PMTPID { 0x20 };
	static int s_count = 0;
	int pmtpos = 13;
	//PMT Program number = 1
	//PMT PID = 0x20
	std::array<uint8_t,17> pat
	    {0x00, 0x00, 0xb0, 0x0d, 0xfe, 0xef, 0xc1, 0x00, 0x00,
	     0x00, 0x00, 0xe0, PMTPID, 0x00, 0x00, 0x00, 0x00};
	std::array<uint8_t,184> pmt
	    {0x00, 0x02, 0xb0, 0x00, 0x00, 0x00, 0xc1, 0x00, 0x00,
	     0x00, 0x00, 0xf0, 0x00};

	//PAT
	pat[10] = prog_num;
	int pos = write_ts_header(0x00, 1, s_count, -1, buf, 0);
	*(uint32_t *)(pat.data()+13)= htonl(crc32_04c11db7(pat.data()+1, 12, 0xffffffff));
	memcpy(buf+pos, pat.data(), 17);
	pos += 17;
	memset(buf+pos, 0xff, TS_SIZE - pos);
	pos = TS_SIZE;
	//PMT
	pos += write_ts_header(PMTPID, 1, s_count, -1, buf+pos, 0);
	for(int i = 0; i <= extcnt; i++) {
		uint8_t type = 0xFF;
		uint32_t pid = 0x1FFF;
		int n =  ext[i-1].strmnum;
		if(i == 0) {
			type = 0x02;
			pid = TS_VIDPID;
		} else if(ext[i-1].type == MPEG_AUDIO) {
			type = 0x04;
			pid = TS_MP2PID + n;
		} else if(ext[i-1].type == AC3) {
			type = 0x81;
			pid = TS_AC3PID + n;
		} else {
			type = 0xff;
			pid = 0x1fff;
		}
		pmt[pmtpos++] = type;
		pmt[pmtpos++] = 0xe0 | (0xff & (pid >> 8));
		pmt[pmtpos++] = 0xff & pid;
		pmt[pmtpos++] = 0xf0;
		if(strlen(ext[i-1].language) == 3) {
			pmt[pmtpos++] = 0x06;
			pmt[pmtpos++] = 0x0a;
			pmt[pmtpos++] = 0x04;
			pmt[pmtpos++] = ext[i-1].language[0];
			pmt[pmtpos++] = ext[i-1].language[1];
			pmt[pmtpos++] = ext[i-1].language[2];
			pmt[pmtpos++] = 0x00;
		} else {
			pmt[pmtpos++] = 0x00;
		}
	}
	pmt[3] = pmtpos + 4/*crc*/ - 3 - 1/*pointer_field*/;
	pmt[5] = prog_num;
	pmt[9] = 0xf0 | (0xff & (TS_VIDPID >> 8));
	pmt[10] = 0xff & TS_VIDPID;
	*(uint32_t *)&pmt[pmtpos] = htonl(crc32_04c11db7(&pmt[1], pmtpos -1,
	                                  0xffffffff));
	pmtpos+=4;
	memcpy(buf+pos, pmt.data(), pmtpos);
	pos += pmtpos;
	memset(buf+pos, 0xff, (2*TS_SIZE) - pos);
//	pos = 2*TS_SIZE;
	s_count = (s_count+1) & 0x0f;
}
