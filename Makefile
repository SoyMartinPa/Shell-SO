myshell: main.c funciones.c \
        funciones.h 
	gcc main.c funciones.c 	-o myshell

clean:
	rm -f myshell
