/*
 * XnView plugin for Thomson MAP format.
 *
 * Format: 
 *    http://collection.thomson.free.fr/code/articles/prehisto_bulletin/page.php?XI=0&XJ=13
 *    https://web.archive.org/web/20181119103222/http://collection.thomson.free.fr/code/articles/prehisto_bulletin/page.php?XI=0&XJ=13
 *
 * Compile with:
 *    gcc -s -O2 -fomit-frame-pointer -shared -Wl,--kill-at -Wall XThomsonMAP.c -o XThomsonMAP.usr
 *
 * Installation:
 *    copy XThomsonMap.usr into the PlugIns directory of XnView.
 *
 * (c) Samuel Devulder, July 2011.
 */

#include <stdio.h>
#include <windows.h>

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) 
{
	switch (ul_reason_for_call) 
	{
	case DLL_PROCESS_ATTACH :
	case DLL_PROCESS_DETACH :
	case DLL_THREAD_ATTACH  :
	case DLL_THREAD_DETACH  :
		break;
	}
  
	return TRUE;
}

#define API 		__stdcall

#define GFP_RGB		0
#define GFP_BGR		1

#define GFP_READ	0x0001
#define GFP_WRITE	0x0002

typedef struct {
	unsigned char red  [256]; 
	unsigned char green[256]; 
	unsigned char blue [256]; 
} GFP_COLORMAP; 

#define RAMA 0
#define RAMB 8000

typedef enum {
	BM40,	/* 40 columns */
	BM4,	/* Bitmap 4 */
	BM16,	/* Bitmap 16 */
	BM80	/* 80 columns */
} VideoMode;

typedef struct {
	FILE*		fp; 
	INT    		width; 
	INT    		height;
	INT    		border;
	VideoMode 	mode;
	unsigned short	pal[16];
	unsigned char	vram[16000];
} THOMSON_DATA; 

BOOL API gfpGetPluginInfo( DWORD version, LPSTR label, INT label_max_size, LPSTR extension, INT extension_max_size, INT * support )
{
	if ( version != 0x0002 )
		return FALSE; 

	strncpy( label, "Thomson MAP", label_max_size ); 
	strncpy( extension, "map", extension_max_size ); 
	*support = GFP_READ /* | GFP_WRITE*/; 
		
	return TRUE; 
}

void * API gfpLoadPictureInit( LPCSTR filename )
{
	THOMSON_DATA * data; 
	unsigned char t;
	int len, len2;
	
	data = calloc( 1, sizeof(THOMSON_DATA) ); 
	if(data == NULL) goto FAIL;
	
	data->fp = fopen( filename, "rb" ); 
	if ( data->fp == NULL ) goto FAIL;
	
	/*
	 * expected: 0, len>>8, len&255, 0, 0
	 */
	if(fread(&t, sizeof(char), 1, data->fp)!=1 || t!=0x00) goto FAIL;
	if(fread(&t, sizeof(char), 1, data->fp)!=1)            goto FAIL;
	len = t;
	if(fread(&t, sizeof(char), 1, data->fp)!=1)            goto FAIL;
	len = (len<<8) | t;
	if(fread(&t, sizeof(char), 1, data->fp)!=1 || t!=0x00) goto FAIL;
	if(fread(&t, sizeof(char), 1, data->fp)!=1 || t!=0x00) goto FAIL;
	
	t = fseek(data->fp, -5L, SEEK_END); if(t) goto FAIL;
	len2 = ftell(data->fp);
	
	if(len2!=len+5) goto FAIL;
	
	/*
         * expected: 255,0,0,0,0
	 */
	if(fread(&t, sizeof(char), 1, data->fp)!=1 || t!=0xff) goto FAIL;
	if(fread(&t, sizeof(char), 1, data->fp)!=1 || t!=0x00) goto FAIL;
	if(fread(&t, sizeof(char), 1, data->fp)!=1 || t!=0x00) goto FAIL;
	if(fread(&t, sizeof(char), 1, data->fp)!=1 || t!=0x00) goto FAIL;
	if(fread(&t, sizeof(char), 1, data->fp)!=1 || t!=0x00) goto FAIL;
	
	/* 
	 * OK: rewind
	 */
	if(fseek(data->fp, 0L, SEEK_SET)!=0) goto FAIL;
	return data;
FAIL:
	if(data!=NULL) free(data);
	return NULL;
}

/**
 * decompress bitmap
 */
static BOOL decomp(THOMSON_DATA *data, BOOL interleaved) {
	short x   = data->width;
	short y   = data->height<<3;
	short u   = 0;
	short u_s = 0;
	short p   = RAMA;
	
	do {
		BOOL literal;
		unsigned char len, val;
		
		if(fread(&len, sizeof(len), 1, data->fp)!=1) return FALSE;
		if(len == 0) {
			literal = TRUE;
			if(fread(&len, sizeof(len), 1, data->fp)!=1) return FALSE;
			if(len==0) return TRUE;
		} else {
			literal = FALSE;
			if(fread(&val, sizeof(val), 1, data->fp)!=1) return FALSE;
		}
		
		do {
			if(literal && fread(&val, sizeof(val), 1, data->fp)!=1) return FALSE;
			data->vram[p + u] = val; u += 40;
			/* last column? */
			if(--y == 0) {
				/* new column */
				y = data->height<<3;
				u = ++u_s;
				/* update plane ptr */
				if(interleaved) {
					if(p == RAMA) {p = RAMB; u = --u_s;} else p = RAMA;
				}
				/* last column ? */
				if(--x == 0) {
					unsigned char buf[2];
					/* closing bytes */
					if(fread(buf, 1, 2, data->fp)!=2) return FALSE;
					if(buf[0]!=0 || buf[1]!=0) return FALSE;

					/* if not interleaved and RAMA, do it once again
					   for RAMB */
					if(!interleaved && p == RAMA) {
						p = RAMB;
						u = u_s = 0;
						x = data->width;
					} else {
						/* otherwise finished */
						return TRUE;
					}
				}
			}
		} while(--len);
	} while (TRUE);
}

BOOL API gfpLoadPictureGetInfo( void * ptr, INT * pictype, INT * width, INT * height, INT * dpi, INT * bits_per_pixel, INT * bytes_per_line, BOOL * has_colormap, LPSTR label, INT label_max_size )
{
	THOMSON_DATA *data = (THOMSON_DATA *)ptr; 
	unsigned char buf[40];

	strncpy(label, "Thomson MAP", label_max_size ); 	
	
	/* skip binary-file header */
	if(fread(buf, sizeof(char), 5, data->fp)!=5) goto FAIL;

	/* video mode */
	if(fread(buf, sizeof(char), 1, data->fp)!=1) goto FAIL;
	switch(buf[0]) {
	case 3: // <== bug found in some bitmaps
	case 0x40: data->mode = BM16; break;
	case 0x80: data->mode = BM80; break;
	case 0x00:
	default:   data->mode = BM40; break;
	}
	
	/* dimension */
	if(fread(buf, sizeof(char), 2, data->fp)!=2) goto FAIL;
	data->width   = buf[0]+1; 
	data->height  = buf[1]+1; 
	
	/* black, mode TO7 */
	data->border  = 6;
	
	/* standard palette */
	data->pal[0]  = 0x000;
	data->pal[1]  = 0x00F;
	data->pal[2]  = 0x0F0;
	data->pal[3]  = 0x0FF;
	data->pal[4]  = 0xF00;
	data->pal[5]  = 0xF0F;
	data->pal[6]  = 0xFF0;
	data->pal[7]  = 0xFFF;
	
	data->pal[8]  = 0x777;
	data->pal[9]  = 0x33A;
	data->pal[10] = 0x3A3;
	data->pal[11] = 0x3AA;
	data->pal[12] = 0xA33;
	data->pal[13] = 0xA3A;
	data->pal[14] = 0xEE7;
	data->pal[15] = 0x07B;
	
	/* decompress */
	if(decomp(data, data->mode!=BM40) == FALSE) goto FAIL;
	
	/* TOSNAP extension ? */
	if(fread(&buf, 40, 1, data->fp) == 1) {
		int i;
		
		/* misaligned? */
		while(buf[38]!=0xA5 || buf[39]!=0x5A) {
			unsigned char t;
			if(fread(&t, sizeof(t), 1, data->fp) == 1) {
				for(i=0; i<39; ++i) buf[i] = buf[i+1];
				buf[39] = t;
			} else break;
		}
		
		/* found */
		if(buf[38]==0xA5 && buf[39]==0x5A) {
			data->border = buf[3];
			if(buf[5] == 2) data->mode = BM4;
			
			// palette 
			for(i=0; i<32; i+=2) {
				short c = buf[6+i]; c = (c<<8) | buf[7+i];
				data->pal[i>>1] = c<0 ? -(c+1) : c;
			}
			strncpy(label, "Thomson TOSNAP", label_max_size ); 
		}
	}
	
	/* set local info */
	*height = data->height<<3;
	*width = data->width<<3;
	*bits_per_pixel = 8; /* 16 cols */
	*has_colormap = TRUE;
	*pictype = GFP_RGB; 
	*dpi = 68; 
	*bytes_per_line = *width; 

	/* correct image size according to video mode */
	if(data->mode == BM80) *height <<= 1;
	if(data->mode == BM16) *width  >>= 1;
	
	return TRUE; 
FAIL:
	return FALSE;
}

BOOL API gfpLoadPictureGetLine( void * ptr, INT line, unsigned char * buffer )
{
	THOMSON_DATA *data = (THOMSON_DATA *)ptr; 
	short p, i, j;
	
	switch(data->mode) {
		case BM16:
		for(p=line*40, i=data->width; i; ++p, i-=2, buffer+=8) {
			buffer[0] = buffer[1] = data->vram[RAMA+p]>>4;
			buffer[2] = buffer[3] = data->vram[RAMA+p]&15;
			buffer[4] = buffer[5] = data->vram[RAMB+p]>>4;
			buffer[6] = buffer[7] = data->vram[RAMB+p]&15;
		}
		break;
		
		case BM80:
		for(p=(line>>1)*80, i=data->width; i; ++p, --i) {
			unsigned char bm = data->vram[((i&1) ? RAMB : RAMA) + (p>>1)];
			for(j=8; j--;) {*buffer++ = (bm&128) ? 1 : 0; bm <<= 1;}
		}
		break;
		
		case BM4:
		for(p=line*40, i=data->width; i; ++p, --i) {
			unsigned char fg = data->vram[RAMA + p];
			unsigned char bg = data->vram[RAMB + p];
			for(j=8; j--;) {
				*buffer++ = ((bg&128) ? 1 : 0) + ((fg&128) ? 2 : 0);
				bg <<= 1; fg <<= 1;
			}
		}
		break;
		
		case BM40:
		default: 
		for(p=line*40, i=data->width; i; ++p, --i) {
			unsigned char fg = data->vram[RAMA + p];
			unsigned char bg = data->vram[RAMB + p] ^ (128+64);
			unsigned char c1 = (bg>>3)&15;
			unsigned char c0 = (bg&7) | ((bg&128)>>4);
			for(j=8; j--;) {
				*buffer++ = (fg&128) ? c1 : c0;
				fg <<= 1;
			}
		}
		break;
	}
	return TRUE; 
}

BOOL API gfpLoadPictureGetColormap( void * ptr, GFP_COLORMAP * cmap )
{
	static int gamma[] = {
		0, 100, 127, 142, 163, 179, 191, 203, 
		215, 223, 231, 239, 243, 247, 251, 255
	};
	THOMSON_DATA * data = (THOMSON_DATA *)ptr; 
	int i;
	
	for(i=0; i<16; ++i) {
		unsigned short pal = data->pal[i];
		cmap->red[i]   = gamma[pal & 15]; pal >>= 4;
		cmap->green[i] = gamma[pal & 15]; pal >>= 4;
		cmap->blue[i]  = gamma[pal & 15];
	}

	return TRUE; 
}

void API gfpLoadPictureExit( void * ptr )
{
	THOMSON_DATA * data = (THOMSON_DATA *)ptr; 

	fclose(data->fp); 
	free(data); 
}

// bits_per_pixel can be 1 to 8, 24, 32
BOOL API gfpSavePictureIsSupported( INT width, INT height, INT bits_per_pixel, BOOL has_colormap )
{
	return FALSE; 
}

void * API gfpSavePictureInit( LPCSTR filename, INT width, INT height, INT bits_per_pixel, INT dpi, INT * picture_type, LPSTR label, INT label_max_size )
{
	return NULL;
}

BOOL API gfpSavePicturePutLine( void * ptr, INT line, const unsigned char * buffer )
{
	return FALSE;
}

void API gfpSavePictureExit( void * ptr )
{
}

