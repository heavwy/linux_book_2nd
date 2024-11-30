all: gpio_int

app : gpio_int.c
	$(CC) -o $@ $^
clean :
	rm gpio_int
deploy : gpio_int
	scp $^ root@10.0.0.10:/home/root/
