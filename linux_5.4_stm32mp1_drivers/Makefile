
obj-m := helloworld_stm32mp1.o helloworld_stm32mp1_with_parameters.o hellokeys_stm32mp1.o helloworld_stm32mp1_with_timing.o helloworld_stm32mp1_char_driver.o helloworld_stm32mp1_class_driver.o misc_stm32mp1_driver.o ledRGB_stm32mp1_platform.o ledRGB_stm32mp1_class_platform.o led_stm32mp1_UIO_platform.o int_stm32mp1_key.o int_stm32mp1_key_wait.o sdma_stm32mp1_m2m.o sdma_stm32mp1_mmap.o linkedlist_stm32mp1_platform.o io_stm32mp1_expander.o ltc3206_stm32mp1_led_class.o i2c_stm32mp1_accel.o ltc2607_stm32mp1_dual_device.o ltc2422_stm32mp1_dual.o ltc2422_stm32mp1_trigger.o adxl345_stm32mp1.o adxl345_stm32mp1_iio.o keyled_stm32mp1_class.o


KERNEL_DIR ?= $(HOME)/STM32MP15-Ecosystem-v2.0.0/Developer-Package/stm32mp1-openstlinux-5.4-dunfell-mp1-20-06-24/sources/arm-ostl-linux-gnueabi/linux-stm32mp-5.4.31-r0/linux-5.4.31

all:
	make -C $(KERNEL_DIR) \
		ARCH=arm CROSS_COMPILE=arm-ostl-linux-gnueabi- \
		M=$(PWD) modules

clean:
	make -C $(KERNEL_DIR) \
		ARCH=arm CROSS_COMPILE=arm-ostl-linux-gnueabi- \
		M=$(PWD) clean

deploy:
	scp *.ko root@10.0.0.10:/home/root/


