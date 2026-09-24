CXX      = g++
CXXFLAGS = -Wall -O2
SRCS     = client.cpp
OBJS     = $(SRCS:.cpp=.o)
RM       = rm -f

# OS detection
ifeq ($(OS),Windows_NT)
    UNAME_S := Windows
else
    UNAME_S := $(shell uname -s 2>/dev/null)
endif

ifeq ($(UNAME_S),Windows)
    TARGET  = clientw.exe
    LDFLAGS = -lws2_32
else ifeq ($(UNAME_S),Linux)
    TARGET  = client
    LDFLAGS =
else
    $(error Unsupported OS: $(UNAME_S))
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(TARGET)

.PHONY: all clean