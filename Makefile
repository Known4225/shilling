all:
	gcc shilling.c -lm -O3 -o shilling.o
val:
	gcc shilling.c -lm -g -o shilling.o
win:
	gcc shilling.c -O3 -o shilling.exe
	shilling.exe
clean:
	rm shilling.o
	rm shilling.exe
