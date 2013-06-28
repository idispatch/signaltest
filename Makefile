.PHONY: clean kill interrupt hangup

PROG=signaltest
PROG_PID=$$(ps | grep $(PROG) | grep -v grep | awk '{print $1}' )

all: $(PROG)

$(PROG): $(PROG).o
	$(CC) -o $@ -lpthread $<
	chmod +x $@

.c.o:
	$(CC) -o $@ -c $<

clean:
	rm -f $(PROG) $(PROG).o

kill:
	kill -KILL $(PROG_PID)

interrupt:
	kill -INT $(PROG_PID)

hangup:
	kill -HUP $(PROG_PID)
