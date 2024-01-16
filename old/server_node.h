#pragma once
#include "audio_queue.h"
#include "../llhttp/build/llhttp.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <thread>
#include <iostream>
#include <fcntl.h>
#include <errno.h>

#include <sstream>
#include <string>
