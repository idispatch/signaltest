all: signaltest

signaltest: signaltest.o
	$(CC) -o $@ -lpthread $<
	chmod +x $@

.c.o:
	$(CC) -o $@ -c $<

clean:
	rm -f signaltest signaltest.o
