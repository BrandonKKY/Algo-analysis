#include <cstdlib>
#include <string>

#include "server.h"

// Usage: ./seamcarve [port] [project-root]
int main(int argc, char** argv) {
    int port = argc > 1 ? std::atoi(argv[1]) : 8080;
    std::string root = argc > 2 ? argv[2] : ".";
    return runServer(port, root);
}
