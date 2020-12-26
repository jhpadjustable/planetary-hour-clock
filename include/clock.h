#include "config.h"


/* nothing much to see below this line */

#define UNUSED __attribute__((unused))

#define BUTTON_SHORT_MINIMUM 1
#define BUTTON_LONG_MINIMUM 16 // (CONF_LONG_PRESS_TIME_MS / CONF_FRAME_TIME_MS)

#define BUTTON_COUNT 2

/* keycode bits */
#define BUTTON_SHORT 1
#define BUTTON_LONG 2
#define BUTTON_REPEAT 16

#define MAIN_APP_SIG 0x772A

#if CONF_DISPLAY_ROTCW
#define VIDEO_WINDOW_FN video_window_rot
#else
#define VIDEO_WINDOW_FN video_window
#endif

#define BUTTON_1_IS_PRESSED (!BUTTON_1_INVERT ^ !gpio_get(BUTTON_1_PORT, BUTTON_1_PIN))
#define BUTTON_2_IS_PRESSED (!BUTTON_2_INVERT ^ !gpio_get(BUTTON_2_PORT, BUTTON_2_PIN))

#define __WFI()                             __asm volatile ("wfi":::"memory")




struct select_item {
	char *label;
	void *value;
	float factor; // factor or sub-unit
	uint16_t flags;
	uint8_t nsubs; // minus one
	union {
		float f;
		int i;
	} min_value, max_value;
	void (*draw_value)(const struct select_item * item,
			   int editsub); // editsub < 0 for not edit mode
	void (*edit_sub)(const struct select_item * item, int repeat_count);
	void (*edit_done)(const struct select_item * item);
	//void (*short_press)();
	//void (*long_press)();
	//void (*next_press)();
	
};

