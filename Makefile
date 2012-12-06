CFLAGS = -g -O -D_FILE_OFFSET_BITS=64
CXXFLAGS = $(CFLAGS)

cc2subtitles : main.o PS_pack.o PES_packet.o mpg2util.o vbi.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lzvbi

clean:
	rm -f *.o decode

