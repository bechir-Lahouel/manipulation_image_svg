CCPP:=g++

CXXFLAGS = $(shell pkg-config --cflags cairo gtk+-3.0 librsvg-2.0 tinyxml2 libcbor)
LDFLAGS = $(shell pkg-config --libs cairo gtk+-3.0 librsvg-2.0 tinyxml2 libcbor)


all : client server new 

client: client.o
	$(CCPP) $(CXXFLAGS) -o $@ $< $(LDFLAGS)  -lm

server: server.o
	$(CCPP) $(CXXFLAGS) -o $@ $< $(LDFLAGS)  -lm

new: new.o
	$(CCPP) $(CXXFLAGS) -o $@ $< $(LDFLAGS)  -lm -ltinyxml2


%.o : %.cpp
	$(CCPP) $(CXXFLAGS) -o $@ -c $<

clean:
	rm -f *.o *~ client server