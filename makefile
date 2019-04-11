all: proxy client

proxy: http.c server.c message.c
	gcc -o $@ $^

client: client.c message.c
	gcc -o $@ $^

clean:
	-rm proxy client