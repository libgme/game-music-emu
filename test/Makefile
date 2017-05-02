# Add -I flags if you need them
CXXFLAGS := -O2
SRCS := basics.c Wave_Writer.cpp
TEST_FILES := ../test.nsf  # Add more files here that you want in testsuite

# We will use LD_PRELOAD later to pick up the right libgme
demo: $(SRCS) Wave_Writer.h
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS) -lgme

test: demo
	parallel --bar ./test.sh {} ::: $(TEST_FILES)

clean:
	rm -f demo
	rm -f new/*.out cur/*.out
	rmdir new cur
