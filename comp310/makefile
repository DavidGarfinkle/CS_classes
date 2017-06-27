LIBS := -lrt -lpthread

all: printer client

printer: printer.c
	gcc $^ -o $@ $(LIBS)

client: client.c
	gcc $^ -o $@ $(LIBS)

clean:
	-rm printer client
