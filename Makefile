# Libraries
LIBS = -lmpg123 -L./llhttp/build -l:libllhttp.a

# Source files
SRCS = src/audio_queue.cpp src/audio_file.cpp src/radio_server.cpp

# Object files
OBJS = $(SRCS:src/%.cpp=obj/%.o)

# Executable name
EXEC = bin/radio_server

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