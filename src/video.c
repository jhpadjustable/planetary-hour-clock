#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "glyphs.h"
#include "video.h"


uint8_t video_buf[8][16];
int video_xpan = -7;

video_ui_state_t video_ui_state = HOUR;

int video_pan_limit = 0;
bool video_pan_return = true;



void
video_clear()
{
	memset(video_buf, 0, sizeof video_buf);
}

void
video_clear_glyph()
{
	for (int i = 0; i < 8; i++)
		video_buf[i][0] = 0;
}

uint8_t
video_window(int y)
{
	int cxl = (video_xpan >> 3);
	int cxr = ((video_xpan + 7) >> 3);

	if (cxr == 0) {
		return (video_buf[y][0] >> (-video_xpan)) & 0xff;
	}
	int r = (video_buf[y][cxl] << 8) | video_buf[y][cxr];
	return (r >> ((~video_xpan) & 7)) & 0xff;
}


uint8_t
video_window_rot(int y)
{
	y = 7 - y;
	y += video_xpan;
	int cxl = (y >> 3);

	if (y < 0)
		return 0;
	uint8_t word = 0;
	for (int x = 0; x < 8; x++) {
		word = (word << 1) + ((video_buf[x][cxl] >> ((~y) & 7)) & 1);
	}
	return word;
}


void video_bitblt(uint8_t *src, int sw, int sh, int dx, int dy)
{
	int swb = (sw + 7) >> 3;
	for (int yc = 0; yc < sh; yc++) {
		int dxc = dx & 7;
		int dxl = (dx >> 3);
		int dxr = ((dx +1) >> 3);
		for (int xc = swb; xc > 0; xc--) {
			uint8_t bb = *src++;
			video_buf[dy + yc][dxl++] |= bb >> (dxc);
			video_buf[dy + yc][dxr++] |= bb << (8 - dxc);
		}
	}
}

void
video_plot(int x, int y, bool draw)
{
	int dxl = (x >> 3);
	if (draw)
		video_buf[y][dxl] |= 1 << (~x & 7);
	else
		video_buf[y][dxl] &= ~(1 << (~x & 7));
}


void
video_pan()
{
	if (video_xpan < video_pan_limit)
		video_xpan++;
	else
		if (video_pan_return && (video_xpan > 0)) {
			video_xpan = -7;
			video_pan_limit = 0;
		}
}

int
video_draw_text(char *buf, int xc)
{
	char c;
	while (*buf) {
		switch (*buf) {
		case 32:
			break;
		default:
			c = (*buf) - GLYPHS_ASCII_LOW;
		
			video_bitblt(&glyphs_ascii_3x5[c * 5], 4, 5, xc, 0);
		}
		buf++;
		xc += 4;
	}
	return xc;
}

// vim: ts=8 sts=8 sw=8 noet : 
