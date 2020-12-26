# Planetary hour clock for STM32 and 8x8 LED matrix

This firmware implements a planetary hour clock with the STM32 processor and MAX7219 8x8 LED matrix driver. It started as a quick holiday project and isn't especially optimized, but works well.

## Intro

Modern industrial society, for its purpose, divides its days into 24 hours beginning at midnight, through noon, and back around to midnight (or vice versa). Some occultists, for their purpose, divide their days into 24 hours beginning at sunset, through sunrise, and back around to sunset (or vice versa). Each hour is said to bear the qualities of (be ruled by) one of the seven classical planets, those moving astronomical objects visible to the unaided eye on Earth, assigned in a repeating sequence from slowest (Saturn) to fastest (Moon). The days are said to be ruled likewise, with each day's first hour ruler being the ruler of the day, as recognized in many Western calendars' names for the days of the week. It is said that activities and intentions that are compatible with the present day and hour are more effective than otherwise.

Due to seasonal and location variations, it is necessary to know the latitude and longitude of the viewer to calculate the Sun's angle and its progress through the viewer's sky, and the time zone in effect for local solar time to zone time translation.

## Prerequisites

The `libopencm3` library, or a symbolic link to it, is expected in the project root directory under that name. Any version from mid-2020 or later should suffice. 

A cross-compiler suite targeting arm-none-eabi is expected in the PATH under the name arm-none-eabi-\*. The Debian stable `{gcc,binutils}-none-arm-eabi` packages are known to work.

Some means of loading the compiled firmware onto the microcontroller is required. The `debug` make target is configured for a [Black Magic Probe](https://github.com/blacksphere/blackmagic/) at `/dev/bmpgdb`.

## Compiling

1. Edit `include/config.h` and `Makefile` if desired
2. `make`

## Installation

If using the Black Magic Probe:

1. `make debug`
2. `load`
3. `quit` and confirm

## Operation

The firmware provides a simple two-button interface.

| # | Button | Press in Clock Mode | Press in Set Mode | Hold
|---| :----: | :-----------------: | :---------------: | :--:
1|TIME/SET|Report current time and date|Edit or accept a setting|Switch between clock mode and set mode
2|MORE|Report current planetary hour interval|Advance to next setting or value|Increase value of current setting

In set mode, the following settings are available. Set mode will timeout 32 seconds after the last button activity, returning to clock mode. TIM and DAT settings are not effective or saved until the last part of the setting is accepted. Other settings are saved and effective immediately.

1. BRT - display brightness 0..9 (minimum..maximum), scrolling speed 0..3 (slow..fast), hour division style NO/SC/SB/SR/FC/FB/FR
2. TZ - hours east of UTC -12..+12
3. TIM - current local time fixed in 24 hour mode
4. DAT - current local date
5. LAT - latitude degrees, minutes, direction
6. LON - longitude degrees, minutes, direction
7. FMT - TIME format with 12/24 hour mode, seconds on/off, date MDY/DMY/YMD/D only
8. BEL - at the stroke of the hour, report the current time or planetary hour interval as a visual chime

The hour division indicates the current part of the planetary hour, analogous to a regular clock's minute hand. xC displays a dot in the lower left or lower right corner, during the first or last 5% of the hour, respectively. xB displays a dot in the bottom row, from left to right in equal portions as the hour proceeds, likewise xR on the right column from top to bottom. x may be S for steady or F for flashing at 1/2Hz.

1. For a correct planetary hour reading, at minimum TZ, TIM, DAT, LAT and LON must be set.
2. On days without a sunrise or sunset, as occurs inside the polar circles near the solstices, no planetary hour can be computed and the Earth symbol &#x1f728; will appear.
3. When the clock starts up, a partial Earth symbol &#x1f728; will appear, with the circle as a progress indicator. During the first start from an unpowered condition, the clock may display a 1/4 circle around the cross for a few seconds while the 32.768kHz oscillator starts and stabilizes. 2/4 and 3/4 circles are reserved for auto-setting upon startup.

## Customization

The following macros are defined in `include/config.h`.

`CONF_DISPLAY_ROTCW`, if defined as 1, programs the display as if rotated 90° clockwise from normal, i.e. the row corresponding to `DIG0` is at the right-hand side. If defined as 0, the display is in the normal orientation, i.e. the row corresponding to `DIG0` is at the top side. (int)

`CONF_SET_TIMEOUT_MS` configures the idle timeout in set mode before reverting to clock mode, in milliseconds. (int)

`CONF_DIV_EDGE_FRACTION` configures the interval from each hour boundary indicated by corner mode, as a fraction of a whole hour. (float)

`CONF_INFO_SECONDS` configures whether seconds are displayed in the current hour interval report. (bool)

`BUTTON_*_PORT` and `BUTTON_*_PIN` configure the pins used for the buttons. `BUTTON_x_INVERT` configures the at-rest sense of the button, e.g. if 1 the input's active sense is logic low.

`M7219_PORT` and `M7219_*_PIN` configure the port and pins used for the MAX7219.

## Implementation notes

I used a "bluepill" board with an apparently genuine STM32F103, and a stackable LED module based on the MAX7219 LED matrix driver. Port B was chosen for 5V tolerance and bit-banged to afford mechanical flexibility. Each of the MAX7219 lines was set to open-drain output and pulled up with a 22kΩ resistor to the +5V rail in order to meet their 3.5V logic threshold. Touch sensors based on the TTP223 chip have been tested successfully. 

Mechanical buttons should work but have not been tested. Each of the button inputs is weakly pulled up or down to the at-rest logic level according to its INVERT setting. The typically slow frame rate of the clock display offers margin against mechanical switch bounce, so no explicit software debouncing is provided.

Signal | Port | Pin 
:-------|:----:|:---:
Button 1|`GPIOA`|0
Button 2|`GPIOA`|6
DOUT|`GPIOB`|7
CLK|`GPIOB`|8
LOAD|`GPIOB`|9

By default, the display is oriented 90° clockwise from normal to suit my hardware. Check the `CONF_DISPLAY_ROTCW` configuration option if that doesn't work for you.

## TODO (PRs welcome)

Add an NMEA parser such as [minmea](https://github.com/kosma/minmea/) for auto-location and auto-clock.
Use a more up-to-date sunrise/sunset algorithm, perhaps based on IAU SOFA code.
Audible chime. (Which tone correspondences? Regardie's _Middle Pillar_ p. 244?)

## Contributing

In the absence of an official style guide, try to follow suit and use your best judgment.

## Acknowledgments

The sunrise/sunset algorithm was published by the US Naval Observatory in 1990, adapted from [this source](https://www.edwilliams.org/sunrise_sunset_algorithm.htm).
The 3x5 character set was inspired by "Screen-80" for the Commodore 64, by Gregg Peele and Kevin Martin, published in COMPUTE!'s Gazette, September 1984.
The semi-standalone mktime routine was derived from Apple's ntp library, derived from NetBSD 4.4.

"If I have seen further, it is by standing on the shoulders of Giants." -Isaac Newton, astrologer

## License

CC-BY-NC-SA 4.0, with commercial rights reserved to the original author.
