myprog: main.c funciones.c \
        funciones.h 
	gcc main.c funciones.c 	-o myprog

clean:
	rm -f myprog
