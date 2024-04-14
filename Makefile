all:
	gcc shilling.c -O3 -o shilling.o
win:
	gcc shilling.c -O3 -o shilling.exe
	shilling.exe
clean:
	rm shilling.o
	rm shilling.exe