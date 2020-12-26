#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <time.h>
#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/f1/bkp.h>
#include <libopencm3/stm32/rtc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/cm3/nvic.h>

#include <libopencm3/cm3/systick.h>

#include "clock.h"
#include "main.h"
#include "glyphs.h"
#include "video.h"

// TODO find the right header file for this
extern time_t my_mktime(struct tm *);


#ifndef M_PI
#define M_PI (3.14159265f)
#endif


#define F_CLOCK_MHZ 24
#define F_CLOCK (F_CLOCK_MHZ * 1000000)
#define CLOCKS_PER_TICK (F_CLOCK / 1000)




float conf_lon = -74.0000f, conf_lat = 40.7134f;
int conf_tz = -300;
bool conf_fmt_ampm = false;
bool conf_fmt_secs = true;
bool conf_fmt_isecs = CONF_INFO_SECONDS;
bool conf_fmt_fullyr = false;
uint8_t conf_bright = 5, conf_speed = 1;
uint8_t conf_fmt_date = 0;
uint8_t conf_div = 2;
uint8_t conf_vchime = 0;


volatile uint64_t ticks;

static const int bit_time_count = (F_CLOCK_MHZ / 6);

static const uint16_t daynames[] = {'SU','MO','TU','WE','TH','FR','SA'};

static const uint16_t legend_ampm[] = { '24','12' };
static const uint16_t legend_secs[] = { 'NO',':S' };
static const uint16_t legend_date[] = { 'MD','DM','YM','D ' };

static const uint8_t bright_map[10] = {0,1,2,3,4,5,7,9,12,15};

static int16_t frame_times[4] = { 150, 125, 100, 80 };


void
gpio_setup(void)
{
        rcc_periph_clock_enable(RCC_GPIOA);
        rcc_periph_clock_enable(RCC_GPIOB);

	gpio_set_mode(BUTTON_1_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, BUTTON_1_PIN);
	(BUTTON_1_INVERT ? gpio_set : gpio_clear)(BUTTON_1_PORT, BUTTON_1_PIN);
	gpio_set_mode(BUTTON_2_PORT, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, BUTTON_2_PIN);
	(BUTTON_2_INVERT ? gpio_set : gpio_clear)(BUTTON_2_PORT, BUTTON_2_PIN);

	gpio_set_mode(M7219_PORT, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN,
		M7219_DO_PIN|M7219_CLK_PIN|M7219_LOAD_PIN);
	gpio_clear(M7219_PORT,
		M7219_DO_PIN|M7219_CLK_PIN|M7219_LOAD_PIN);

}

#define RCC_CLOCK_SETUP_BY_MHZ_(f) rcc_clock_setup_in_hse_8mhz_out_ ## f ## mhz()
#define RCC_CLOCK_SETUP_BY_MHZ(f) RCC_CLOCK_SETUP_BY_MHZ_(f)
void clock_setup(void)
{
        RCC_CLOCK_SETUP_BY_MHZ(F_CLOCK_MHZ);
        // keep AFIO clock on always
        rcc_periph_clock_enable(RCC_AFIO);

}

void systick_setup(void)
{
        systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
        systick_clear();
        systick_set_reload(CLOCKS_PER_TICK - 1);

        systick_interrupt_enable();
        systick_counter_enable();
}


static volatile int rtc_first_status = 0;


bool
app_bkpconfig_exists(void)
{
	return BKP_DR10 == MAIN_APP_SIG;
}


void
app_bkpconfig_write(void)
{
	BKP_DR1 = (int16_t) lrintf(conf_lon * 60.f);
	BKP_DR2 = (int16_t) lrintf(conf_lat * 60.f);
	BKP_DR3 = (int16_t) ((conf_tz / 15) & 0x3f)
		| ((conf_bright & 0xf) << 6)
		| ((conf_speed & 0x3) << 10)
		| ((conf_div & 0x7) << 13);
	BKP_DR4 = (conf_fmt_ampm ? (1 << 0) : 0)
		| (conf_fmt_secs ? (1 << 1) : 0)
		| ((conf_fmt_date & 3) << 2)
		| ((conf_vchime & 3) << 4);
	BKP_DR10 = MAIN_APP_SIG;
}

void
app_bkpconfig_read(void)
{
	conf_lon = (float)((int16_t) BKP_DR1) / 60.f;
	conf_lat = (float)((int16_t) BKP_DR2) / 60.f;
	uint16_t dr3 = BKP_DR3;
	conf_tz = ((dr3 & 0x1f) - (dr3 & 0x20)) * 15;
	conf_bright = (dr3 >> 6) & 0xf;
	conf_speed = (dr3 >> 10) & 0x3;
	conf_div = (dr3 >> 13) & 0x7;
	uint16_t dr4 = BKP_DR4;
	conf_fmt_ampm = (dr4 >> 0) & 1;
	conf_fmt_secs = (dr4 >> 1) & 1;
	conf_fmt_date = (dr4 >> 2) & 3;
	conf_vchime = (dr4 >> 4) & 3;
}

void
rtc_setup(void)
{
	rtc_first_status = RTC_CRL;

	rtc_auto_awake(RCC_LSE, 0x7fff);

	if (!app_bkpconfig_exists())
		app_bkpconfig_write();
	
	app_bkpconfig_read();
}

static inline void
bit_time(void)
{
	for (volatile int x = bit_time_count; x; x--);
}

void
m7219_send(uint16_t msg)
{
	bit_time();
	for (int bits = 15; bits >= 0; bits--) {
		if (msg & (1 << bits))
			gpio_set(M7219_PORT, M7219_DO_PIN);
		else
			gpio_clear(M7219_PORT, M7219_DO_PIN);
		bit_time();
		gpio_set(M7219_PORT, M7219_CLK_PIN);
		bit_time();
		gpio_clear(M7219_PORT, M7219_CLK_PIN);
		bit_time();
	}
	gpio_set(M7219_PORT, M7219_LOAD_PIN);
	bit_time();
	gpio_clear(M7219_PORT, M7219_LOAD_PIN);
}


void
sys_tick_handler(void)
{
	ticks++;
}


static char
to_base_36(int i)
{
	return (i < 10 ? '0' : 'A' - 10) + i;
}

float
norm(float L, float interval)
{
	while (L > interval)
		L -= interval;
	while (L < 0.0f)
		L += interval;
	return L;
}

static float
todeg(float a)
{
	return a * 180.f / M_PI;
}

// doy=1 for January 1
static float
torad(float a)
{
	return a * M_PI / 180.f;
}

// doy=1 for January 1
int
sunrise_sunset(int doy, float lat, float lon, bool rise, int tz, float *outT)
{
	float lngHour = lon / 15.0;
	float t = doy + (((rise ? 6.f : 18.f) - lngHour) / 24);
	float M = (0.9856f * t) - 3.289f;
	float Mrad = torad(M);
	float L = norm(M + (1.916f * sinf(Mrad)) + (0.020f * sinf(2 * Mrad)) + 282.634f, 360.f);
	float Lrad = torad(L);
	float RA = norm(todeg(atanf(0.91764f * tanf(Lrad))), 360);
	float Lq = (floor(L / 90.f)) * 90.f;
	float RAq = (floor(RA / 90.f)) * 90.f;
	RA = RA + (Lq - RAq);
	RA /= 15.f;
	float sinDec = 0.39782f * sinf(Lrad);
	float cosDec = cosf(asinf(sinDec));
	float cosH = -(sinDec * sinf(torad(lat))) / (cosDec * cosf(torad(lat)));
	if (rise ? (cosH > 1.0f) : (cosH < -1.0f))
		return -1; // error: no rise or set
	float H = todeg(acosf(cosH));
	if (rise)
		H = 360.f - H;
	H /= 15.f;
	float T = H + RA - (0.06571f * t) - 6.622f;
	float LT = norm(T - lngHour + (tz / 60), 24.f);
	if (outT)
		*outT = LT;
	return 0;
}



static uint32_t button_down_times[2] = {0, 0};
static uint8_t button_down_pending[2] = {0, 0};

void
input_handle_button(int button, bool is_pressed)
{
	if (button > BUTTON_COUNT)
		return;
	uint32_t down = button_down_times[button];
	if (is_pressed) {
		if (down == BUTTON_LONG_MINIMUM) {
			// recognize a long
			button_down_pending[button] |= BUTTON_LONG/* | BUTTON_REPEAT*/;
		} else if (down > BUTTON_LONG_MINIMUM) {
			// recognize a repeat
			button_down_pending[button] |= BUTTON_REPEAT;
		}
		++button_down_times[button];
	} else {
		if (down >= BUTTON_SHORT_MINIMUM && down <= BUTTON_LONG_MINIMUM) {
			button_down_pending[button] |= BUTTON_SHORT;
			// recognize a short press
		}
		button_down_times[button] = 0;
	}
}

int
input_button_consume(int button, int mask)
{
	if (button > BUTTON_COUNT)
		return 0;
	int ret = 0;
	if ((ret = (button_down_pending[button] & mask)))
		button_down_pending[button] &= ~ret;
	return ret;
}

int
input_button_peek(int button, int mask)
{
	if (button > BUTTON_COUNT)
		return 0;
	return ((button_down_pending[button] & mask));
}

void
input_button_flush_all(void)
{
	memset(button_down_pending, 0, sizeof button_down_pending);
}

/*
 * process BUTTON_1 and BUTTON_2 inputs, recognize and
 * dispatch events thereto
 */
void
input_process(void)
{
	if (BUTTON_1_IS_PRESSED && BUTTON_2_IS_PRESSED)
		return;

	input_handle_button(0, BUTTON_1_IS_PRESSED);
	input_handle_button(1, BUTTON_2_IS_PRESSED);
}

void
m7219_send_brightness(void)
{
	m7219_send(0xA00 + bright_map[conf_bright]);
}

void
video_hour_begin(void)
{
	video_ui_state = HOUR;
	input_button_flush_all();
	video_pan_limit = 0;
	video_pan_return = false;
	m7219_send_brightness();
	app_bkpconfig_write();

}


float
phour_get(time_t gmtt, float lat, float lon, int tz, float *outStart, float *outEnd)
{
	struct tm gmt;
	float hrise, hset, hnow;
	float phour;

	bool night = false;

	time_t localtt = gmtt + (tz * 60);
	
	gmtime_r(&localtt, &gmt);

	hnow = norm((float)(gmt.tm_hour) + (gmt.tm_min / 60.f) + (gmt.tm_sec / 3600.f), 24);


	if (sunrise_sunset(gmt.tm_yday + 1, lat, lon, true, tz, &hrise))
		return -1.f;
	if (sunrise_sunset(gmt.tm_yday + 1, lat, lon, false, tz, &hset))
		return -1.f;

	if (!(hnow >= hrise && hnow <= hset)) {
		night = true;
		// reconfigure day as night
		if (hnow > hset) { // evening
			hrise = hset;
			if (sunrise_sunset(gmt.tm_yday + 2, lat, lon, true, tz, &hset))
				return -1.f;
			hset += 24.f;
		} else { // morning
			hset = hrise + 24.f;
			if (sunrise_sunset(gmt.tm_yday, lat, lon, false, tz, &hrise))
				return -1.f;
			hnow += 24.f;
			gmt.tm_wday = (gmt.tm_wday + 6) % 7;
		}
	}

	float span = hset - hrise;
	phour = 12.f * (hnow - hrise) / span;

	if (outStart)
		*outStart = norm((floor(phour) * span / 12.f) + hrise, 24);
	if (outEnd)
		*outEnd = norm((floor(phour + 1.f) * span / 12.f) + hrise, 24);

	if (night)
		phour += 12.f;

	phour = norm(phour + (gmt.tm_wday * 3 + 3), 7);

	return phour;
}

float video_hour_current_start, video_hour_current_end;

void
str_2digits(char **buf, int n)
{
	*(*buf)++ = to_base_36(n / 10);
	*(*buf)++ = (n % 10) + '0';
}

void
str_2digits_lno(char **buf, int n)
{
	if (n >= 100)
		*(*buf)++ = to_base_36(n / 10);
	*(*buf)++ = (n % 10) + '0';
}


void
video_hour_show_current(void)
{
	char bufb[30];
	char *buf = bufb;

	struct tm gmt;
	time_t gmtt = rtc_get_counter_val();

	gmtime_r(&gmtt, &gmt);
	//strcpy(buf, "RISE ");
	//buf += strlen(buf);

	float hrise = video_hour_current_start, hset = video_hour_current_end;

	//sunrise_sunset(gmt.tm_yday + 1, conf_lat, conf_lon, true, conf_tz, &hrise);
	//sunrise_sunset(gmt.tm_yday + 1, conf_lat, conf_lon, false, conf_tz, &hset);
	

	int ihr = (int)(hrise), imr = (int)(hrise * 60.f) % 60;
	int ihs = (int)(hset), ims = (int)(hset * 60.f) % 60;

	*buf++ = ' ';
	str_2digits(&buf, ihr);
	*buf++ = ':';
	str_2digits(&buf, imr);
	if (conf_fmt_isecs) {
		int isr = (int)(hrise * 3600.f) % 60;
		*buf++ = ':';
		str_2digits(&buf, isr);
	}
	*buf++ = '-';
	str_2digits(&buf, ihs);
	*buf++ = ':';
	str_2digits(&buf, ims);
	if (conf_fmt_isecs) {
		int iss = (int)(hset * 3600.f) % 60;
		*buf++ = ':';
		str_2digits(&buf, iss);
	}
	*buf = 0;

	video_clear();

	video_pan_limit = video_draw_text(bufb, 8);
	video_pan_return = true;
}


void
video_hour_show_time_date(void)
{
	char bufb[30];
	char *buf = bufb;

	struct tm gmt;
	time_t gmtt = rtc_get_counter_val() + (conf_tz * 60);

	gmtime_r(&gmtt, &gmt);

	uint16_t wd = daynames[gmt.tm_wday];
	*buf++ = wd >> 8;
	*buf++ = wd;

	int hr = gmt.tm_hour;
	if (conf_fmt_ampm)
		hr = 1 + ((hr + 11) % 12);

	*buf++ = ' ';
	if (!(conf_fmt_ampm && hr < 10))
	str_2digits(&buf, hr);
	*buf++ = ':';
	str_2digits(&buf, gmt.tm_min);
	if (conf_fmt_secs) {
		*buf++ = ':';
		str_2digits(&buf, gmt.tm_sec);
	}
	if (conf_fmt_ampm) {
		*buf++ = ' ';
		*buf++ = (gmt.tm_hour >= 12) ? 'P' : 'A';
		*buf++ = 'M';
	}

	*buf++ = ' ';

	void
	append_mon(void)
	{
		str_2digits_lno(&buf, 1 + gmt.tm_mon);
	}

	void
	append_day(void)
	{
		str_2digits_lno(&buf, gmt.tm_mday);
	}

	inline void
	append_year(void)
	{
		if (conf_fmt_fullyr) {
			str_2digits(&buf, (gmt.tm_year + 1900) / 100);
		}
		str_2digits(&buf, gmt.tm_year % 100);
	}

	switch (conf_fmt_date) {
	case 0:
		append_mon();
		*buf++ = '/';
		append_day();
		*buf++ = '/';
		append_year();
		*buf = 0;
		break;
	case 1:
		append_day();
		*buf++ = '-';
		append_mon();
		*buf++ = '-';
		append_year();
		break;
	case 2:
		append_year();
		*buf++ = '-';
		append_mon();
		*buf++ = '-';
		append_day();
		break;
	case 3:
		append_day();
		break;
	}

	*buf = 0;

	video_clear();

	video_pan_limit = video_draw_text(bufb, 8);
	video_pan_return = true;
}


#define SELECT_EDITSUB_POINT(sub, xc) \
	if (editsub == sub) { \
		video_pan_limit = xc; \
		if (video_xpan > video_pan_limit) \
			video_xpan = video_pan_limit; \
		video_pan_return = false; \
	} \

#define SELECT_EDITSUB_END(xc) \
	if (editsub < 0) { \
		video_pan_limit = xc; \
		video_xpan = -7; \
		video_pan_return = true; \
	}

#define SELECT_ITEM_FLAGS_SIGN_EW	1
#define SELECT_ITEM_FLAGS_SIGN_NS	2

static uint32_t select_last_activity = 0;

static int select_index;
static int select_editsub_index;

void
select_item_deg_edit_sub(const struct select_item * item, int repeat_count)
{
	float value = *((float *)item->value);
	bool neg;

	float maxval = (item->max_value.f ? item->max_value.f : 180.f);

	// sign retaining, do val
	if ((neg = (value < 0.f)))
		value = -value;

	switch (select_editsub_index) {
	case 0: //degree
		value += 1.0f;
		while (value >= maxval)
			value -= maxval;
		break;
	case 1: { //minute
		float oldvalue = value;
		float incr = (1.0f / (item->factor ? item->factor : 1));
		value = (value + incr);
		value = (value - floor(value)) + floor(oldvalue);
		break;
	}
	case 2: //hemisphere
		if (repeat_count == 0)
			neg = !neg;
		break;
	}

	*((float *)item->value) = value * (neg ? -1.f : 1.f);

	app_bkpconfig_write();
	select_item_deg_draw_value(item, select_editsub_index);
}

void
select_item_deg_draw_value(const struct select_item * item, int editsub)
{
	char bufb[30];
	char *buf = bufb;
	float value = *((float *)item->value);
	bool neg;

	// sign retaining, do val
	if ((neg = (value < 0.f)))
		value = -value;

	int val = (int)(value);
	int fval = (int)((value - trunc(value)) * item->factor);

	video_clear();

	strcpy(buf, item->label);
	buf += strlen(buf);

	*buf++ = ' ';
	*buf = 0;
	int xc = video_draw_text(bufb, 0);

	buf = bufb;
	str_2digits(&buf, val);
	*buf++ = '&';

	*buf = 0;

	SELECT_EDITSUB_POINT(0, xc);

	xc = video_draw_text(bufb, xc); 
	buf = bufb;

	if (item->factor) {
		str_2digits(&buf, fval);
		*buf++ = '\'';
	}

	*buf = 0;

	SELECT_EDITSUB_POINT(1, xc);

	xc = video_draw_text(bufb, xc);
	buf = bufb;

	if (item->flags & SELECT_ITEM_FLAGS_SIGN_EW)
		*buf++ = (neg ? 'W' : 'E');
	else if (item->flags & SELECT_ITEM_FLAGS_SIGN_NS)
		*buf++ = (neg ? 'S' : 'N');

	*buf = 0;

	SELECT_EDITSUB_POINT(2, xc);

	xc = video_draw_text(bufb, xc);
	buf = bufb;

	SELECT_EDITSUB_END(xc);

}


void
select_item_tz_edit_sub(const struct select_item * item, UNUSED int repeat_count)
{
	// check case too
	int *valuep = (int *)item->value;

	*valuep += 60;

	if (*valuep > item->max_value.i)
		*valuep = item->min_value.i;

	app_bkpconfig_write();
	select_item_int_draw_value(item, 0);
}

void
select_item_int_draw_value(const struct select_item * item, int editsub)
{
	char bufb[30];
	char *buf = bufb;
	bool neg;

	int val = *((int *)item->value) / (item->factor ? item->factor : 1);

	if ((neg = (val < 0))) {
		val = -val;
	}

	video_clear();

	strcpy(buf, item->label);
	buf += strlen(buf);

	*buf++ = ' ';
	*buf = 0;
	int xc = video_draw_text(bufb, 0);

	buf = bufb;
	if (neg) {
		if (val >= 10 && val < 20)
			*buf++ = '`'; // -1
		else if (val >= 20 && val < 30)
			*buf++ = 'a'; // -2
		else
			*buf++ = '-';
	} else
		*buf++ = to_base_36(val / 10);

	*buf++ = (val % 10) + '0';
	*buf = 0;

	SELECT_EDITSUB_POINT(0, xc);

	xc = video_draw_text(bufb, xc);

	SELECT_EDITSUB_END(xc);
}


void
select_item_display_edit_sub(const struct select_item * item, UNUSED int repeat_count)
{
	switch (select_editsub_index) {
	case 0: //brightness
		conf_bright++;
		if (conf_bright > 9)
			conf_bright = 0;
		m7219_send_brightness();
		break;
	case 1: //speed
		conf_speed++;
		if (conf_speed > 3)
			conf_speed = 0;
		break;
	case 2: //tick
		conf_div++;
		if (conf_div > 6)
			conf_div = 0;
		break;

	}

	app_bkpconfig_write();
	select_item_display_draw_value(item, select_editsub_index);
}

void
select_item_display_draw_value(const struct select_item * item, int editsub)
{
	char bufb[30];
	char *buf = bufb;

	static const char * const legend_div = "NOSCSBSRFCFBFR";

	video_clear();

	strcpy(buf, item->label);
	buf += strlen(buf);

	*buf = 0;

	int xc = video_draw_text(bufb, 0);

	SELECT_EDITSUB_POINT(0, xc);

	buf = bufb;

	*buf++ = ' ';
	*buf++ = (conf_bright % 10) + '0';
	*buf++ = ' ';
	*buf++ = 'S';
	*buf++ = 'P';
	*buf++ = 'D';
	*buf = 0;

	xc = video_draw_text(bufb, xc);

	SELECT_EDITSUB_POINT(1, xc);

	buf = bufb;

	*buf++ = ' ';
	*buf++ = conf_speed + '1';
	*buf++ = ' ';
	*buf++ = 'T';
	*buf++ = 'I';
	*buf++ = 'K';
	*buf++ = ' ';
	*buf = 0;

	xc = video_draw_text(bufb, xc);

	SELECT_EDITSUB_POINT(2, xc);

	buf = bufb;

	*buf++ = legend_div[conf_div * 2];
	*buf++ = legend_div[conf_div * 2 + 1];
	*buf = 0;

	xc = video_draw_text(bufb, xc);

	SELECT_EDITSUB_END(xc);
}


void
select_item_chime_edit_sub(const struct select_item * item, UNUSED int repeat_count)
{
	switch (select_editsub_index) {
	case 0: //visual
		conf_vchime++;
		if (conf_vchime > 2)
			conf_vchime = 0;
		break;
	}

	app_bkpconfig_write();
	select_item_chime_draw_value(item, select_editsub_index);
}

void
select_item_chime_draw_value(const struct select_item * item, int editsub)
{
	char bufb[30];
	char *buf = bufb;

	static const char * const legend_vchime = "NOTIPH";

	video_clear();

	strcpy(buf, item->label);
	buf += strlen(buf);

	*buf++ = ' ';
	*buf = 0;

	int xc = video_draw_text(bufb, 0);

	SELECT_EDITSUB_POINT(0, xc);

	buf = bufb;

	*buf++ = legend_vchime[conf_vchime * 2];
	*buf++ = legend_vchime[conf_vchime * 2 + 1];
	*buf = 0;

	xc = video_draw_text(bufb, xc);

	SELECT_EDITSUB_END(xc);
}


struct tm select_item_time_buf;

void
select_item_time_edit_sub(const struct select_item * item, UNUSED int repeat_count)
{
	struct tm *tbuf = ((struct tm *)item->value);

	switch (select_editsub_index) {
	case 0: //hour
		tbuf->tm_hour++;
		while (tbuf->tm_hour >= 24)
			tbuf->tm_hour -= 24;
		break;
	case 1: //min
		tbuf->tm_min++;
		while (tbuf->tm_min >= 60)
			tbuf->tm_min -= 60;
		break;
	case 2: //sec
		tbuf->tm_sec++;
		while (tbuf->tm_sec >= 60)
			tbuf->tm_sec -= 60;
		break;

	}
	select_item_time_draw_value(item, select_editsub_index);
}

void
select_item_time_edit_done(const struct select_item * item)
{
	struct tm *tbuf = ((struct tm *)item->value);
	struct tm ltm;

	time_t gmtt = rtc_get_counter_val() + (conf_tz * 60);
	gmtime_r(&gmtt, &ltm);

	ltm.tm_hour = tbuf->tm_hour;
	ltm.tm_min = tbuf->tm_min;
	ltm.tm_sec = tbuf->tm_sec;

	gmtt = my_mktime(&ltm);

	rtc_set_counter_val(gmtt - conf_tz * 60);
}

void
select_item_time_draw_value(const struct select_item * item, int editsub)
{
	char bufb[30];
	char *buf = bufb;

	struct tm *tbuf = ((struct tm *)item->value);

	if (editsub < 0) {
		time_t gmtt = rtc_get_counter_val() + (conf_tz * 60);
		gmtime_r(&gmtt, tbuf);
	}

	video_clear();

	strcpy(buf, item->label);
	buf += strlen(buf);

	*buf++ = ' ';
	*buf = 0;
	int xc = video_draw_text(bufb, 0);

	SELECT_EDITSUB_POINT(0, xc);

	buf = bufb;

	str_2digits(&buf, tbuf->tm_hour);
	*buf++ = ':';
	*buf = 0;

	xc = video_draw_text(bufb, xc);

	SELECT_EDITSUB_POINT(1, xc);

	buf = bufb;

	str_2digits(&buf, tbuf->tm_min);
	*buf++ = ':';
	*buf = 0;

	xc = video_draw_text(bufb, xc);
	SELECT_EDITSUB_POINT(2, xc);

	buf = bufb;

	str_2digits(&buf, tbuf->tm_sec);
	*buf = 0;

	xc = video_draw_text(bufb, xc);
	SELECT_EDITSUB_END(xc);
}


void
select_item_date_edit_sub(const struct select_item * item, UNUSED int repeat_count)
{
	struct tm *tbuf = ((struct tm *)item->value);

	static const uint8_t days_in_month[2][12] = {
		{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
		{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
	};

	switch (select_editsub_index) {
	case 0: //yr
		tbuf->tm_year++;
		while (tbuf->tm_year >= 200)
			tbuf->tm_year -= 100;
		break;

	case 1: //mo
		tbuf->tm_mon++;
		while (tbuf->tm_mon >= 12)
			tbuf->tm_mon -= 12;
		break;
	case 2: //dy
		tbuf->tm_mday++;
		if (tbuf->tm_mday > days_in_month[tbuf->tm_year % 4 ? 1 : 0][tbuf->tm_mon])
			tbuf->tm_mday = 1;
		break;
	}
	select_item_date_draw_value(item, select_editsub_index);
}

void
select_item_date_edit_done(const struct select_item * item)
{
	struct tm *tbuf = ((struct tm *)item->value);
	struct tm ltm;

	time_t gmtt = rtc_get_counter_val() + (conf_tz * 60);
	gmtime_r(&gmtt, &ltm);

	ltm.tm_year = tbuf->tm_year;
	ltm.tm_mon = tbuf->tm_mon;
	ltm.tm_mday = tbuf->tm_mday;

	gmtt = my_mktime(&ltm);

	rtc_set_counter_val(gmtt - conf_tz * 60);
}

void
select_item_date_draw_value(const struct select_item * item, int editsub)
{
	char bufb[30];
	char *buf = bufb;

	struct tm *tbuf = ((struct tm *)item->value);

	if (editsub < 0) {
		time_t gmtt = rtc_get_counter_val() + (conf_tz * 60);
		gmtime_r(&gmtt, tbuf);
	}

	video_clear();

	strcpy(buf, item->label);
	buf += strlen(buf);

	*buf++ = ' ';
	*buf++ = 'Y';
	*buf++ = '2';
	*buf++ = '0';
	*buf = 0;
	int xc = video_draw_text(bufb, 0);

	SELECT_EDITSUB_POINT(0, xc);

	buf = bufb;

	int modyr = tbuf->tm_year - 100;

	str_2digits(&buf, modyr);
	*buf++ = ' ';
	*buf++ = 'M';
	*buf = 0;

	xc = video_draw_text(bufb, xc);
	SELECT_EDITSUB_POINT(1, xc);

	buf = bufb;

	int modmo = tbuf->tm_mon + 1;

	str_2digits(&buf, modmo);
	*buf++ = ' ';
	*buf++ = 'D';
	*buf = 0;

	xc = video_draw_text(bufb, xc);
	SELECT_EDITSUB_POINT(2, xc);

	buf = bufb;

	str_2digits(&buf, tbuf->tm_mday);
	*buf = 0;

	xc = video_draw_text(bufb, xc);
	SELECT_EDITSUB_END(xc);
}

//int select_item_fmt_tmp;

void
select_item_fmt_edit_sub(const struct select_item * item, UNUSED int repeat_count)
{
	
	switch (select_editsub_index) {
	case 0: //ampm
		conf_fmt_ampm = !conf_fmt_ampm;
		break;
	case 1: //secs
		conf_fmt_secs = !conf_fmt_secs;
		break;
	case 2: //dform
		if (++conf_fmt_date > 3)
			conf_fmt_date = 0;
		break;
	}
	app_bkpconfig_write();
	select_item_fmt_draw_value(item, select_editsub_index);
}


void
select_item_fmt_draw_value(const struct select_item * item, int editsub)
{
	char bufb[30];
	char *buf = bufb;

	video_clear();

	strcpy(buf, item->label);
	buf += strlen(buf);

	*buf++ = ' ';
	*buf++ = 'H';
	*buf++ = 'R';
	*buf++ = ' ';
	*buf = 0;

	int xc = video_draw_text(bufb, 0);
	SELECT_EDITSUB_POINT(0, xc);

	buf = bufb;

	*buf++ = (legend_ampm[conf_fmt_ampm ? 1 : 0] >> 8);
	*buf++ = (legend_ampm[conf_fmt_ampm ? 1 : 0]);

	*buf++ = ' ';
	*buf++ = 'S';
	*buf++ = 'E';
	*buf++ = 'C';
	*buf++ = ' ';
	*buf = 0;

	xc = video_draw_text(bufb, xc);
	SELECT_EDITSUB_POINT(1, xc);

	buf = bufb;

	*buf++ = (legend_secs[conf_fmt_secs ? 1 : 0] >> 8);
	*buf++ = (legend_secs[conf_fmt_secs ? 1 : 0]);

	*buf++ = ' ';
	*buf++ = 'D';
	*buf++ = 'A';
	*buf++ = 'T';
	*buf++ = ' ';
	*buf = 0;

	xc = video_draw_text(bufb, xc);
	SELECT_EDITSUB_POINT(2, xc);

	buf = bufb;

	*buf++ = (legend_date[conf_fmt_date] >> 8);
	*buf++ = (legend_date[conf_fmt_date]);
	*buf = 0;

	xc = video_draw_text(bufb, xc);
	SELECT_EDITSUB_END(xc);
}




const struct select_item select_items[] = {
	{
		.label = "BRT",
		.nsubs = 2,
		.draw_value = select_item_display_draw_value,
		.edit_sub = &select_item_display_edit_sub
	},
	{
		.label = "TZ",
		.value = &conf_tz,
		.factor = 60,
		.min_value = {i: -720},
		.max_value = {i: 720},
		.draw_value = select_item_int_draw_value,
		.edit_sub = &select_item_tz_edit_sub
	},
	{
		.label = "TIM",
		.value = &select_item_time_buf,
		.factor = 60,
		.nsubs = 2,
		.draw_value = select_item_time_draw_value,
		.edit_sub = &select_item_time_edit_sub,
		.edit_done = &select_item_time_edit_done
	},
	{
		.label = "DAT",
		.value = &select_item_time_buf,
		.factor = 60,
		.nsubs = 2,
		.draw_value = select_item_date_draw_value,
		.edit_sub = &select_item_date_edit_sub,
		.edit_done = &select_item_date_edit_done
	},
	{
		.label = "LAT",
		.value = &conf_lat,
		.factor = 60,
		.nsubs = 2,
		.max_value = {f: 90.f},
		.flags = SELECT_ITEM_FLAGS_SIGN_NS,
		.draw_value = select_item_deg_draw_value,
		.edit_sub = &select_item_deg_edit_sub
	},
	{
		.label = "LON",
		.value = &conf_lon,
		.factor = 60,
		.nsubs = 2,
		.flags = SELECT_ITEM_FLAGS_SIGN_EW,
		.draw_value = select_item_deg_draw_value,
		.edit_sub = &select_item_deg_edit_sub
	},
	{
		.label = "FMT",
		.value = &conf_fmt_ampm,
		.nsubs = 2,
		.draw_value = select_item_fmt_draw_value,
		.edit_sub = &select_item_fmt_edit_sub
	},
	{
		.label = "BEL",
		//.nsubs = 2,
		.draw_value = select_item_chime_draw_value,
		.edit_sub = &select_item_chime_edit_sub
	},
	{
	}
};

void
video_select_begin(void)
{
	video_ui_state = SELECT;
	input_button_flush_all();
	video_pan_limit = 0;
	video_pan_return = false;
	select_index = 0;
	select_editsub_index = -1;

	const struct select_item *si = &select_items[select_index];
	si->draw_value(si, select_editsub_index);

	select_last_activity = ticks;
	
}

void
video_select_draw_value(void)
{
	const struct select_item *si = &select_items[select_index];
	si->draw_value(si, select_editsub_index);

}

void
video_select_next(void)
{
	select_last_activity = ticks;
	if (!select_items[++select_index].draw_value)
		select_index = 0;
	const struct select_item *si = &select_items[select_index];
	si->draw_value(si, -1);
}

void
video_select_edit(void)
{
	const struct select_item *si = &select_items[select_index];
	if (select_editsub_index >= si->nsubs) {
		video_ui_state = SELECT;
		select_editsub_index = -1;
		return video_select_next();
	}

	video_ui_state = EDIT;
	input_button_flush_all();
	video_pan_limit = 0;
	video_pan_return = false;
	select_editsub_index++;

	si->draw_value(si, select_editsub_index);

	select_last_activity = ticks;
}

void
video_edit_accept(void)
{
	const struct select_item *si = &select_items[select_index];
	if (select_editsub_index >= si->nsubs) {
		if (si->edit_done)
			si->edit_done(si);
	}
	video_ui_state = SELECT;
	input_button_flush_all();

	// return to select or edit mode, as needed
	video_select_edit();
}

void
video_edit_change(int repeat_count)
{
	const struct select_item *si = &select_items[select_index];
	si->edit_sub(si, repeat_count);

	select_last_activity = ticks;
}

void
video_play(void)
{
	static volatile uint32_t lasttime = 0;
	static int repeat_count = 0;
	uint64_t sreload = ticks;
	int32_t sticks = systick_get_value();

	switch (video_ui_state) {
	case HOUR:
		video_pan_return = true;
		if (input_button_consume(0, BUTTON_SHORT)) {
			video_hour_show_current();
		}
		if (input_button_consume(1, BUTTON_SHORT)) {
			video_hour_show_time_date();
		}
		if (input_button_consume(1, BUTTON_LONG)) {
			video_select_begin();
		}
		break;
	case SELECT:
		if (select_last_activity && (ticks - CONF_SET_TIMEOUT_MS - select_last_activity < INT_MAX)) {
			video_hour_begin();
			break;
		}
		if (input_button_consume(1, BUTTON_LONG)) {
			video_hour_begin();
		}
		if (input_button_consume(1, BUTTON_SHORT)) {
			video_select_edit();
		}
		if (input_button_consume(0, BUTTON_SHORT)) {
			video_select_next();
		}
		break;
	case EDIT:
		if (select_last_activity && (ticks - 32000 - select_last_activity < INT_MAX)) {
			video_hour_begin();
			break;
		}
		if (input_button_consume(1, BUTTON_SHORT)) {
			video_edit_accept();
		}
		if (input_button_consume(1, BUTTON_LONG)) {
			video_hour_begin();
		}
		if (input_button_peek(0, BUTTON_REPEAT))
			++repeat_count;
		else
			repeat_count = 0;
		if (input_button_consume(0, BUTTON_SHORT | BUTTON_REPEAT)) {
			video_edit_change(repeat_count);
		}
		break;
	}

	static int lasthour = -1;
	int phour;
	float phourf;
	switch (video_ui_state) {
	case HOUR:
		phourf = phour_get(rtc_get_counter_val(), conf_lat, conf_lon, conf_tz,
			&video_hour_current_start, &video_hour_current_end);

		phour = (int) phourf;
		phourf -= phour;

		if (phour < 0)
			phour = 7;

		video_clear_glyph();
		//video_bitblt(glyphs_by_num[(ticks / 1000) % (sizeof(glyphs_by_num)/sizeof(glyphs_by_num[0]))], 7, 8, 0, 0);
		video_bitblt(glyphs_by_num[phour], 7, 8, 0, 0);

		if ((conf_div < 4) || (ticks % 2000 > 1000 && phour < 7)) {
			switch (conf_div) {
			case 1:
			case 4:
				if (phourf < CONF_DIV_EDGE_FRACTION)
					video_plot(0, 7, true);
				else if (phourf > (1 - CONF_DIV_EDGE_FRACTION))
					video_plot(7, 7, true);
				break;
			case 2:
			case 5:
				video_plot((int)(phourf * 8), 7, true);
				break;
			case 3:
			case 6:
				video_plot(7, (int)(phourf * 8), true);
				break;
			}
		}

		if (lasthour == -1)
			lasthour = phour;

		if (lasthour != phour) {
			switch (conf_vchime) {
			case 1:
				video_hour_show_time_date();
				break;
			case 2:
				video_hour_show_current();
				break;
			}
		}

		lasthour = phour;
		break;
	case EDIT:
		video_clear();
		video_select_draw_value();
		break;
	case SELECT:
		// nothing to do, redraw on editor demand
		break;
	}


	sreload = ticks - sreload;
	sticks -= systick_get_value();
	lasttime = (sreload * CLOCKS_PER_TICK) + sticks;
}




void
m7219_setup(void)
{
	m7219_send(0xC00);
	m7219_send(0xC00);
	m7219_send(0xA0F);
	m7219_send(0xB07);
	m7219_send(0xF00);
	m7219_send(0xC01);
}

void
m7219_show_splash(int idx)
{
	for (int i = 0; i < 7; i++)
		m7219_send(((i + 1) << 8) + bootimgs_by_num[idx][i]);
	m7219_send(0x800);
}



int
main(void)
{
	clock_setup();
	gpio_setup();
	systick_setup();

	m7219_setup();

	m7219_show_splash(1);
	rtc_setup();

	m7219_send_brightness();
	m7219_show_splash(2);
	// TODO acquire time and location from a GPS or other receiver
	m7219_show_splash(3);

	uint32_t nextst = ticks + 1000;
	for(;;) {
		while(nextst - ticks < INT_MAX) __WFI();

		input_process();
		video_pan();
		video_play();

		for (int i = 0; i < 8; i++)
			m7219_send(((i + 1) << 8) + VIDEO_WINDOW_FN(i));

		nextst += frame_times[conf_speed];
	}
}


// vim: ts=8 sts=8 sw=8 noet : 
