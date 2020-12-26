/* This file was automatically generated.  Do not edit! */

#pragma once

#include <sys/types.h>
#include <stdbool.h>

typedef enum {
	HOUR = 0,
	SELECT,
	EDIT
} video_ui_state_t;

#undef INTERFACE
void video_play(void);
int video_draw_text(char *buf,int xc);
void video_pan(void);
void video_bitblt(uint8_t *src,int sw,int sh,int dx,int dy);
uint8_t video_window(int y);
void video_clear_glyph(void);
void video_clear(void);
extern bool video_pan_return;
extern int video_pan_limit;
extern video_ui_state_t video_ui_state;
extern int video_xpan;
extern uint8_t video_buf[8][16];
