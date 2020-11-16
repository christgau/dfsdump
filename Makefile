CFLAGS=-O0 -g -Wall
LDFLAGS=-ldfs -ldaos -ldaos_common -lgurt -luuid

dcat: dcat.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS)  -o $@

.PHONY: clean

clean:
	rm -f dcat
