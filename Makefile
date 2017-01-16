
CFLAGS += -Wall

all: bgrep

clean:
	$(RM) bgrep test/data

test: bgrep
	( cd test && ./bgrep-test.sh )

.PHONY: clean test
