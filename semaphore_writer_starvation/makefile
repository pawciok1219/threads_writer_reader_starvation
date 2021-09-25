object = main.o
all: program
program: $(object)
	gcc $(object) -o program -pthread
.PHONY: clean
clean:
	rm -f *.o program
