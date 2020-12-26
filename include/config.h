/* headline configuration */

#define CONF_DISPLAY_ROTCW 1
#define CONF_SET_TIMEOUT_MS 32000
#define CONF_DIV_EDGE_FRACTION (1.f/20.f)
#define CONF_INFO_SECONDS false


/* pin assignments */

#define BUTTON_1_PORT GPIOA
#define BUTTON_1_PIN GPIO0
// define _INVERT as 0 for active high, 1 for active low
#define BUTTON_1_INVERT 0

#define BUTTON_2_PORT GPIOA
#define BUTTON_2_PIN GPIO6
#define BUTTON_2_INVERT 0

#define M7219_PORT GPIOB
#define M7219_DO_PIN GPIO7
#define M7219_CLK_PIN GPIO8
#define M7219_LOAD_PIN GPIO9




