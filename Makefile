
CFLAGS += -Wall

all: bgrep

clean:
	$(RM) bgrep test/data

.PHONY: clean
