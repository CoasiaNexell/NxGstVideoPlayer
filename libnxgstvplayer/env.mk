#########################################################################
# 	Build Options
#########################################################################
OPTS		:= -Wall -O2 -Wextra -Wcast-align -Wno-unused-parameter \
				-Wshadow -Wwrite-strings -Wcast-qual -fno-strict-aliasing \
				-fstrict-overflow -fsigned-char -fno-omit-frame-pointer \
				-fno-optimize-sibling-calls

CFLAGS 	 	+= $(OPTS)
CXXFLAGS 	+= $(CFLAGS) -Wnon-virtual-dtor
AFLAGS 		:=

ARFLAGS		+= crv
LDFLAGS  	+=
LIBRARY		:=

#########################################################################
# 	Generic Rules
#########################################################################
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

%.o: %.s
	$(AS) $(AFLAGS) $(INCLUDE) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c -o $@ $<
