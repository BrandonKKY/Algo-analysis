// This work was completed with the assistance of Claude Sonnet 5.5.

#pragma once
#include <string>

// Minimal HTTP server (one thread per request) that serves index.html,
// /images/*, GET /api/images, POST /api/carve and POST /api/stop (stops every
// running carve). Blocks until killed.
// `root` is the project directory containing index.html and images/.
int runServer(int port, const std::string& root);
