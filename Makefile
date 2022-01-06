CC=gcc

LFLAGS = 
CFLAGS  = -Wall -Wextra $(shell pkg-config --cflags sdl2)

SOURCES=mod.c audio.c

OUTPUT = output

OBJECTS=$(SOURCES:%.c=$(OUTPUT)/%.o)
EXECUTABLE=libannie.a

all: $(OUTPUT) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	ar -rc $@ $(OBJECTS)

$(OUTPUT):
	@[ -d $(OUTPUT) ] || mkdir -p $(OUTPUT)

$(OUTPUT)/%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -rf $(OUTPUT)