# Libraries
LIBS = -lmpg123 -lcrypto -lssl

# Source files
SRCS = src/radio.cpp src/audio/audio_file.cpp src/audio/audio_queue.cpp

# Object files
objs = $(SRCS:.cpp=.o)

# Executable name
EXEC = radio

# Directories
OBJDIR = obj
BINDIR = bin

all: directories $(EXEC)

directories: $(OBJDIR) $(BINDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(EXEC): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(EXEC) $(OBJS) $(LIBS)

$(OBJDIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $<  -o $@

clean:
	$(RM) -r $(OBJDIR) $(BINDIR)

refresh: clean all