#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <gpiod.h>

int main(int argc, char *argv[])
{
	struct gpiod_chip *output_chip;
	struct gpiod_line *output_line;
	int line_value = 1;
	int flash = 10;
	int ret;

	/* open /dev/gpiochip0 */
	output_chip = gpiod_chip_open_by_number(0);
	if (!output_chip)
		return -1;

	/* get PA14 pin (green LED) */
	output_line = gpiod_chip_get_line(output_chip, 14);
	if(!output_line) {
		gpiod_chip_close(output_chip);
		return -1;
	}

	/* config PA14 as output and set a description */
	if (gpiod_line_request_output(output_line, "green Led",
				  GPIOD_LINE_ACTIVE_STATE_HIGH) == -1) {
		gpiod_line_release(output_line);
		gpiod_chip_close(output_chip);
		return -1;
	}

	/* toggle 10 times the LED */
	for (int i=0; i < flash; i++) {
		line_value = !line_value;
		ret = gpiod_line_set_value(output_line, line_value);
		if (ret == -1) {
			ret = -errno;
			gpiod_line_release(output_line);
			gpiod_chip_close(output_chip);
			return ret;
		}
		sleep(1);
	}

	gpiod_line_release(output_line);
	gpiod_chip_close(output_chip);

	return 0;
}
