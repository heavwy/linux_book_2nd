

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#define BUFFER_LENGHT 128

/* Declare physical PA addresses offsets */
#define GPIOA_MODER_offset 0x00 /* 0x00 offset */
#define GPIOA_OTYPER_offset 0x04 /* 0x04 offset -> 0: Output push-pull */
#define GPIOA_PUPDR_offset 0x0c  /* 0x0C offset -> 01:Pull-up, 10:Pull down */
#define GPIOA_BSRR_offset 0x18 /* 0x18 offset */

/* Green LED2: PA10 */
#define GPIOA_MODER_BSRR10_SET_Pos (10U)
#define GPIOA_MODER_BSRR10_CLEAR_Pos (26U)
#define GPIOA_MODER_MODER10_Pos (20U)
#define GPIOA_MODER_MODER10_0 (0x1U << GPIOA_MODER_MODER10_Pos)
#define GPIOA_MODER_MODER10_1 (0x2U << GPIOA_MODER_MODER10_Pos)
#define GPIOA_PUPDR_PUPDR10_0 (0x1U << GPIOA_MODER_MODER10_Pos)
#define GPIOA_PUPDR_PUPDR10_1 (0x2U << GPIOA_MODER_MODER10_Pos)

#define GPIOA_OTYPER_OTYPER10_pos (10U)
#define GPIOA_OTYPER_OTYPER10_Msk (0x1U << GPIOA_OTYPER_OTYPER10_pos)

#define GPIOA_PA10_SET_BSRR_Mask (1U << GPIOA_MODER_BSRR10_SET_Pos)
#define GPIOA_PA10_CLEAR_BSRR_Mask (1U << GPIOA_MODER_BSRR10_CLEAR_Pos)

#define UIO_SIZE "/sys/class/uio/uio0/maps/map0/size"

int main()
{
	int ret, devuio_fd;
	unsigned int uio_size;
	void *tmp;
	//int GPIOA_BSRR_read, GPIOA_BSRR_write; 
	void *demo_driver_map;
	char sendstring[BUFFER_LENGHT];
	char *led_on = "on";
	char *led_off = "off";
	char *Exit = "exit";

	printf("Starting led example\n");
	devuio_fd = open("/dev/uio0", O_RDWR | O_SYNC);
	if (devuio_fd < 0){
		perror("Failed to open the device");
		exit(EXIT_FAILURE);
	}

	/* read the size that has to be mapped */
	FILE *size_fp = fopen(UIO_SIZE, "r");
	fscanf(size_fp, "0x%08X", &uio_size);
	fclose(size_fp);

	/* do the mapping */
	demo_driver_map = mmap(NULL, uio_size, PROT_READ | PROT_WRITE, MAP_SHARED, devuio_fd, 0);
	if(demo_driver_map == MAP_FAILED) {
		perror("devuio mmap");
		close(devuio_fd);
		exit(EXIT_FAILURE);
	}
	
	/* control the LED RED */
	do {
		printf("Enter led value: on, off, or exit :\n");
		scanf("%[^\n]%*c", sendstring);
		if(strncmp(led_on, sendstring, 2) == 0)
		{
			*(int *)(demo_driver_map + GPIOA_BSRR_offset) = GPIOA_PA10_CLEAR_BSRR_Mask;
		}
		else if(strncmp(led_off, sendstring, 3) == 0)
		{
			*(int *)(demo_driver_map + GPIOA_BSRR_offset) = GPIOA_PA10_SET_BSRR_Mask;
		}
		else if(strncmp(Exit, sendstring, 4) == 0)
		printf("Exit application\n");

		else {
			printf("Bad value\n");
			*(int *)(demo_driver_map + GPIOA_BSRR_offset) = GPIOA_PA10_SET_BSRR_Mask;
			return -EINVAL;
		}

	} while(strncmp(sendstring, "exit", strlen(sendstring)));

	ret = munmap(demo_driver_map, uio_size);
	if(ret < 0) {
		perror("devuio munmap");
		close(devuio_fd);
		exit(EXIT_FAILURE);
	}

	close(devuio_fd);
	printf("Application termined\n");
	exit(EXIT_SUCCESS);
}

