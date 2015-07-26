/*
    Copyright (C) 2015  Damien Zammit

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

*/

#include "ptfformat.h"
using namespace std;

PTFFormat::PTFFormat() {
}

PTFFormat::~PTFFormat() {
	if (ptfunxored) {
		free(ptfunxored);
	}
}

/* Return values:	0            success
			0x01 to 0xff value of missing lut
			-1           could not open file as ptf
*/
int
PTFFormat::load(std::string path) {
	FILE *fp;
	unsigned char xxor[256];
	unsigned char ct;
	unsigned char px;
	uint16_t i;
	int j;
	int li;
	int inv;
	unsigned char message;

	if (! (fp = fopen(path.c_str(), "rb"))) {
		return -1;
	}
	
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	if (len < 0x40) {
		fclose(fp);
		return -1;
	}
	fseek(fp, 0x40, SEEK_SET);
	fread(&c0, 1, 1, fp);
	fread(&c1, 1, 1, fp);
	
	if (! (ptfunxored = (unsigned char*) malloc(len * sizeof(unsigned char)))) {
		/* Silently fail -- out of memory*/
		fclose(fp);
		ptfunxored = 0;
		return -1;
	}

	i = 2;

	fseek(fp, 0x0, SEEK_SET);
	
	switch (c0) {
	case 0x00:
		// Success! easy one
		xxor[0] = c0;
		xxor[1] = c1;
		for (i = 2; i < 64; i++) {
			xxor[i] = (xxor[i-1] + c1 - c0) & 0xff;
		}
		px = xxor[0];
		fread(&ct, 1, 1, fp);
		message = px ^ ct;
		ptfunxored[0] = message;
		px  = xxor[1];
		fread(&ct, 1, 1, fp);
		message = px ^ ct;
		ptfunxored[1] = message;
		i = 2;
		j = 2;
		while (fread(&ct, 1, 1, fp) != 0) {
			if (i%64 == 0) {
				i = 0;
			}
			message = xxor[i] ^ ct;
			ptfunxored[j] = message;
			i++;
			j++;
		}
		break;
	case 0x80:
		//Success! easy two
		xxor[0] = c0;
		xxor[1] = c1;
		for (i = 2; i < 256; i++) {
			if (i%64 == 0) {
				xxor[i] = c0;
			} else {
				xxor[i] = ((xxor[i-1] + c1 - c0) & 0xff);
			}
		}
		for (i = 0; i < 64; i++) {
			xxor[i] ^= 0x80;
		}
		for (i = 128; i < 192; i++) {
			xxor[i] ^= 0x80;
		}
		px = xxor[0];
		fread(&ct, 1, 1, fp);
		message = px ^ ct;
		ptfunxored[0] = message;
		px  = xxor[1];
		fread(&ct, 1, 1, fp);
		message = px ^ ct;
		ptfunxored[1] = message;
		i = 2;
		j = 2;
		while (fread(&ct, 1, 1, fp) != 0) {
			if (i%256 == 0) {
				i = 0;
			}
			message = xxor[i] ^ ct;
			ptfunxored[j] = message;
			i++;
			j++;
		}
		break;
	case 0x40:
	case 0xc0:
		li = c1;
		if (ptflutseenwild[li]) {
			//Success! [li]
		} else {
			//Can't find lookup table for c1=li
			free(ptfunxored);
			fclose(fp);
			return li;
		}
		xxor[0] = c0;
		xxor[1] = c1;
		for (i = 2; i < 256; i++) {
			if (i%64 == 0) {
				xxor[i] = c0;
			} else {
				xxor[i] = ((xxor[i-1] + c1 - c0) & 0xff);
			}
		}
		for (i = 0; i < 64; i++) {
			xxor[i] ^= (ptflut[li][i] * 0x40);
		}
		inv = 0;
		for (i = 128; i < 192; i++) {
			inv = (ptflut[li][i-128] == 3) ? 1 : 3;
			xxor[i] ^= (inv * 0x40);
		}
		for (i = 192; i < 256; i++) {
			xxor[i] ^= 0x80;
		}
		px = xxor[0];
		fread(&ct, 1, 1, fp);
		message = px ^ ct;
		ptfunxored[0] = message;
		px  = xxor[1];
		fread(&ct, 1, 1, fp);
		message = px ^ ct;
		ptfunxored[1] = message;
		i = 2;
		j = 2;
		while (fread(&ct, 1, 1, fp) != 0) {
			if (i%256 == 0) {
				i = 0;
			}
			message = xxor[i] ^ ct;
			ptfunxored[j] = message;
			i++;
			j++;
		}
		break;
		break;
	default:
		//Should not happen, failed c[0] c[1]
		return -1;
		break;
	}
	fclose(fp);
	
	parse();
	return 0;
}

void
PTFFormat::parse(void) {
	int i;
	int j;
	int k;
	int l;
	int type;

	// Find end of wav file list
	k = 0;
	while (k < len) {
		if (		(ptfunxored[k  ] == 0xff) &&
				(ptfunxored[k+1] == 0xff) &&
				(ptfunxored[k+2] == 0xff) &&
				(ptfunxored[k+3] == 0xff)) {
			break;
		}	
		k++;
	}

	j = 0;
	l = 0;

	// Find actual wav names
	char wavname[256];
	for (i = k; i > 4; i--) {
		if (		(ptfunxored[i  ] == 'W') &&
				(ptfunxored[i-1] == 'A') &&
				(ptfunxored[i-2] == 'V') &&
				(ptfunxored[i-3] == 'E')) {
			j = i-4;
			l = 0;
			while (ptfunxored[j] != '\0') {
				wavname[l] = ptfunxored[j];
				l++;
				j--;
			}
			wavname[l] = 0;
			std::string wave = string(wavname);
			std::reverse(wave.begin(), wave.end());
			wav_t f = { wave, 0, 0, 0 };
			actualwavs.push_back(f);
		}
	}

	uint32_t startbytes = 0;
	uint32_t lengthbytes = 0;
	uint32_t offsetbytes = 0;

	while (		(ptfunxored[k  ] != 0x5a) &&
			(ptfunxored[k+1] != 0x01) &&
			(ptfunxored[k+2] != 0x00)) {
		k++;
	}
	for (i = k; i < len-70; i++) {
		if (		(ptfunxored[i  ] == 0x5a) &&
				(ptfunxored[i+1] == 0x0c) &&
				(ptfunxored[i+2] == 0x00)) {
			char name[65] = {0};
			for (j = i + 13; j < i + 13 + 64; j++) {
				name[j-(i+13)] = ptfunxored[j];
				if (ptfunxored[j] == 0x00) {
					type = 1;
					name[j-(i+13)] = 0x00;
					break;
				}
				if (ptfunxored[j] == 0x01) {
					type = 0;
					name[j-(i+13)] = 0x00;
					break;
				}
			}
			lengthbytes = (ptfunxored[j+3] & 0xf0) >> 4;
			startbytes = (ptfunxored[j+2] & 0xf0) >> 4;
			offsetbytes = (ptfunxored[j+3] & 0xf);
			
			if (type == 1) {
				uint32_t start = 0;
				switch (startbytes) {
				case 4:
					start |= (uint32_t)(ptfunxored[j+8] << 24);
				case 3:
					start |= (uint32_t)(ptfunxored[j+7] << 16);
				case 2:
					start |= (uint32_t)(ptfunxored[j+6] << 8);
				case 1:
					start |= (uint32_t)(ptfunxored[j+5]);
				default:
					break;
				}
				j+=startbytes;
				uint32_t length = 0;
				switch (lengthbytes) {
				case 4:
					length |= (uint32_t)(ptfunxored[j+8] << 24);
				case 3:
					length |= (uint32_t)(ptfunxored[j+7] << 16);
				case 2:
					length |= (uint32_t)(ptfunxored[j+6] << 8);
				case 1:
					length |= (uint32_t)(ptfunxored[j+5]);
				default:
					break;
				}
				j+=lengthbytes;
				uint32_t sampleoffset = 0;
				switch (offsetbytes) {
				case 4:
					sampleoffset |= (uint32_t)(ptfunxored[j+8] << 24);
				case 3:
					sampleoffset |= (uint32_t)(ptfunxored[j+7] << 16);
				case 2:
					sampleoffset |= (uint32_t)(ptfunxored[j+6] << 8);
				case 1:
					sampleoffset |= (uint32_t)(ptfunxored[j+5]);
				default:
					break;
				}
				std::string filename = string(name) + ".wav";
				wav_t f = { 
					filename,
					(int64_t)start,
					(int64_t)sampleoffset,
					(int64_t)length,
				};

				vector<wav_t>::iterator begin = this->actualwavs.begin();
				vector<wav_t>::iterator finish = this->actualwavs.end();
				// Add file to list only if it is an actual wav
				if (std::find(begin, finish, f) != finish) {
					f.filename = string(f.filename);
					int64_t tmp = f.posabsolute;
					f.posabsolute = f.length;
					f.length = tmp;
					this->audiofiles.push_back(f);
				} else {
					char str2[256] = {0};
					char *pch;
					char *str = &filename[0];
					pch = strrchr(str, '-');
					if (pch == 0) {
						continue;
					}
					strncpy(str2, str, pch - str);
					string regionaudiofile = string(str2) + ".wav";
					f.filename = regionaudiofile;
					region_t r = {
						name,
						f
					};
					this->regions.push_back(r);
				}
			}
		}
	}

	while (k < len) {
		if (		(ptfunxored[k  ] == 0x5a) &&
				(ptfunxored[k+1] == 0x06)) {
			break;
		}	
		k++;
	}
	//  Regions
	uint32_t offset;
	for (vector<region_t>::iterator of = 
			this->regions.begin();
			of != this->regions.end();) {

		if (	(ptfunxored[k  ] == 0x5a) &&
			(ptfunxored[k+1] == 0x07)) {
			
			j = k+16;

			//offset
			offset = 0;
			startbytes = (ptfunxored[k+3] & 0xf0) >> 4;

			switch (startbytes) {
			case 4:
				offset |= (uint32_t)(ptfunxored[j+3] << 24);
			case 3:
				offset |= (uint32_t)(ptfunxored[j+2] << 16);
			case 2:
				offset |= (uint32_t)(ptfunxored[j+1] << 8);
			case 1:
				offset |= (uint32_t)(ptfunxored[j]);
			default:
				break;
			}
			of->wave.posabsolute = (uint32_t)offset;
			of++;
		}
		k++;
	}
}