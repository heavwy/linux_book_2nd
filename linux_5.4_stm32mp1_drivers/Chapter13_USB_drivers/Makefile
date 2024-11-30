
obj-m := usb_led.o usb_ltc3206.o usb_urb_int_led.o


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


