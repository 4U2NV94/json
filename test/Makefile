debug=0
CXXFLAGS=-c -std=c++11 -Wall -Wno-c++98-compat -O2 -I../include
LDFLAGS=-O2
SOURCES=main.cpp
BASE_NAMES=$(basename $(SOURCES))
OBJECTS=$(BASE_NAMES:=.o)
EXECUTABLE=test

all: $(EXECUTABLE)
ifeq ($(debug),1)
all: CXXFLAGS+=-DDEBUG -g
endif

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@ -fprofile-arcs -ftest-coverage

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ -fprofile-arcs -ftest-coverage

.PHONY: clean
clean:
ifeq ($(OS),Windows_NT)
	-del /f /q "$(EXECUTABLE).exe" "*.o"
else
	$(RM) $(EXECUTABLE) *.o $(EXECUTABLE).exe
endif