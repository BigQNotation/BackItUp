all: project4

project4: BackItUp.c
	gcc -o BackItUp BackItUp.c -lpthread

clean: BackItUp.c
	rm BackItUp.c

.PHONY: run

