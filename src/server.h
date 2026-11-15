#pragma once
#include <string>

// Minimal single-threaded HTTP server that serves index.html, /images/*,
// GET /api/images and POST /api/carve. Blocks until killed.
// `root` is the project directory containing index.html and images/.
int runServer(int port, const std::string& root);
