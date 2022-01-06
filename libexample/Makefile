CC=gcc

LFLAGS = 
CFLAGS  = -Wall -Wextra
LIBS = $(shell pkg-config --libs sdl2) -lm -L. -lannie

SOURCES=main.c

OUTPUT = output

OBJECTS=$(SOURCES:%.c=$(OUTPUT)/%.o)
EXECUTABLE=player

all: $(OUTPUT) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LFLAGS) $(LIBS) -o $@

$(OUTPUT):
	@[ -d $(OUTPUT) ] || mkdir -p $(OUTPUT)

$(OUTPUT)/%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -rf $(OUTPUT)