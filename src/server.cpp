// This work was completed with the assistance of Claude Sonnet 5.5.

#include "server.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>

#include "seam_methods.h"

namespace {

// Largest request body accepted (the raw RGB bytes of the image), 64 MB.
constexpr size_t kMaxBody = 64u * 1024 * 1024;

// One parsed HTTP request: method ("GET"/"POST"), path, ?query values,
// headers (names lower-cased) and the body.
struct Request {
    std::string method, path;
    std::map<std::string, std::string> query, headers;
    std::string body;
};

// Turns URL text back into plain text: "%20" -> ' ', "+" -> ' '.
std::string urlDecode(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size()) {
            o += static_cast<char>(std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16));
            i += 2;
        } else if (s[i] == '+') {
            o += ' ';
        } else {
            o += s[i];
        }
    }
    return o;
}

// Reads one HTTP request from the socket into `req`.
// Returns false if the connection closes early or the request is too large.
bool readRequest(int fd, Request& req) {
    std::string buf;
    char tmp[8192];
    size_t headerEnd;
    // 1. Read until the blank line that ends the headers.
    while ((headerEnd = buf.find("\r\n\r\n")) == std::string::npos) {
        ssize_t n = recv(fd, tmp, sizeof tmp, 0);
        if (n <= 0 || buf.size() > 65536) return false;
        buf.append(tmp, n);
    }
    // 2. First line is "METHOD /path?query HTTP/1.1"; the rest are "Name: value" headers.
    std::istringstream hs(buf.substr(0, headerEnd));
    std::string line, target;
    std::getline(hs, line);
    std::istringstream rl(line);
    rl >> req.method >> target;
    while (std::getline(hs, line)) {
        size_t c = line.find(':');
        if (c == std::string::npos) continue;
        std::string k = line.substr(0, c), v = line.substr(c + 1);
        v.erase(0, v.find_first_not_of(" \t"));
        while (!v.empty() && (v.back() == '\r' || v.back() == ' ')) v.pop_back();
        std::transform(k.begin(), k.end(), k.begin(), ::tolower);
        req.headers[k] = v;
    }
    // 3. Split "/path?a=1&b=2" into the path and the query values.
    size_t q = target.find('?');
    req.path = urlDecode(target.substr(0, q));
    if (q != std::string::npos) {
        std::istringstream qs(target.substr(q + 1));
        std::string kv;
        while (std::getline(qs, kv, '&')) {
            size_t e = kv.find('=');
            if (e != std::string::npos) req.query[urlDecode(kv.substr(0, e))] = urlDecode(kv.substr(e + 1));
        }
    }
    // 4. Read the body: Content-Length bytes, some of which may already be in `buf`.
    size_t len = req.headers.count("content-length") ? std::strtoull(req.headers["content-length"].c_str(), nullptr, 10) : 0;
    if (len > kMaxBody) return false;
    req.body = buf.substr(headerEnd + 4);
    while (req.body.size() < len) {
        ssize_t n = recv(fd, tmp, sizeof tmp, 0);
        if (n <= 0) return false;
        req.body.append(tmp, n);
    }
    return true;
}

// Sends an HTTP response with the given status code, content type and body.
void respond(int fd, int status, const std::string& type, const std::string& body) {
    const char* text = status == 200 ? "OK" : status == 400 ? "Bad Request" : status == 404 ? "Not Found"
                       : status == 409 ? "Conflict" : status == 501 ? "Not Implemented" : "Error";
    std::ostringstream h;
    h << "HTTP/1.1 " << status << ' ' << text << "\r\nContent-Type: " << type
      << "\r\nContent-Length: " << body.size() << "\r\nConnection: close\r\n\r\n";
    std::string out = h.str() + body;
    size_t sent = 0;
    while (sent < out.size()) {
        ssize_t n = send(fd, out.data() + sent, out.size() - sent, MSG_NOSIGNAL);
        if (n <= 0) break;
        sent += n;
    }
}

// Makes text safe to put inside a JSON string: escapes " and \, drops control characters.
std::string jsonEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        if (static_cast<unsigned char>(c) >= 0x20) o += c;
    }
    return o;
}

// Content type to send for a file, chosen from its extension.
std::string mimeFor(const std::string& p) {
    auto ends = [&](const char* e) { size_t n = std::strlen(e); return p.size() >= n && p.compare(p.size() - n, n, e) == 0; };
    if (ends(".html")) return "text/html; charset=utf-8";
    if (ends(".png")) return "image/png";
    if (ends(".jpg") || ends(".jpeg")) return "image/jpeg";
    if (ends(".gif")) return "image/gif";
    if (ends(".webp")) return "image/webp";
    if (ends(".bmp")) return "image/bmp";
    return "application/octet-stream";
}

// True if the file name ends in an image extension (case-insensitive).
bool isImageName(const std::string& n) {
    std::string l = n;
    std::transform(l.begin(), l.end(), l.begin(), ::tolower);
    for (const char* e : {".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp"})
        if (l.size() > std::strlen(e) && l.compare(l.size() - std::strlen(e), std::strlen(e), e) == 0) return true;
    return false;
}

// Sends a file from disk (index.html or an image), or 404 if it does not exist.
void serveFile(int fd, const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return respond(fd, 404, "text/plain", "not found");
    std::stringstream ss;
    ss << f.rdbuf();
    respond(fd, 200, mimeFor(path), ss.str());
}

// GET /api/images: sends the sorted names of the images in the images/ folder
// as a JSON list, for the "Sample image" dropdown.
void listImages(int fd, const std::string& root) {
    std::vector<std::string> names;
    if (DIR* d = opendir((root + "/images").c_str())) {
        while (dirent* e = readdir(d))
            if (isImageName(e->d_name)) names.push_back(e->d_name);
        closedir(d);
    }
    std::sort(names.begin(), names.end());
    std::string j = "[";
    for (size_t i = 0; i < names.size(); ++i) j += (i ? "," : "") + std::string("\"") + jsonEscape(names[i]) + "\"";
    respond(fd, 200, "application/json", j + "]");
}

// POST /api/carve?method=<id>&w=<W>&h=<H>&count=<N>, body = raw RGB bytes.
// Runs the chosen seam finder `count` times and replies with every seam as
// JSON: {"seams":[[x for each row], ...]}. The page then animates them.
void handleCarve(int fd, const Request& req) {
    auto get = [&](const char* k) { auto it = req.query.find(k); return it == req.query.end() ? std::string() : it->second; };
    int w = std::atoi(get("w").c_str()), h = std::atoi(get("h").c_str()), count = std::atoi(get("count").c_str());
    auto fail = [&](int code, const std::string& msg) {
        respond(fd, code, "application/json", "{\"error\":\"" + jsonEscape(msg) + "\"}");
    };
    if (w <= 0 || h <= 0 || req.body.size() != static_cast<size_t>(w) * h * 3) return fail(400, "bad image dimensions / body size");
    SeamFinder finder = lookupMethod(get("method"));
    if (!finder) return fail(400, "unknown method");

    Image img(w, h, std::vector<uint8_t>(req.body.begin(), req.body.end()));
    std::vector<Seam> seams;
    std::string err;
    // 409 = the user pressed Stop; 501 = the method failed (e.g. image too small).
    if (!carve(img, finder, count, seams, err)) return fail(err == "stopped" ? 409 : 501, err);

    // Build the JSON reply by hand: one array of column numbers per seam.
    std::string j = "{\"seams\":[";
    for (size_t i = 0; i < seams.size(); ++i) {
        j += i ? ",[" : "[";
        for (size_t y = 0; y < seams[i].size(); ++y) j += (y ? "," : "") + std::to_string(seams[i][y]);
        j += "]";
    }
    respond(fd, 200, "application/json", j + "]}");
}

// Reads one request and sends it to the matching handler (a small router).
void handle(int fd, const std::string& root) {
    Request req;
    if (!readRequest(fd, req)) return respond(fd, 400, "text/plain", "bad request");
    // Refuse ".." so a request cannot read files outside the project folder.
    if (req.path.find("..") != std::string::npos) return respond(fd, 400, "text/plain", "bad path");

    if (req.method == "GET" && (req.path == "/" || req.path == "/index.html")) return serveFile(fd, root + "/index.html");
    if (req.method == "GET" && req.path == "/api/images") return listImages(fd, root);
    if (req.method == "GET" && req.path.rfind("/images/", 0) == 0) return serveFile(fd, root + req.path);
    if (req.method == "POST" && req.path == "/api/carve") return handleCarve(fd, req);
    if (req.method == "POST" && req.path == "/api/stop") {
        stopAll();
        return respond(fd, 200, "application/json", "{\"stopped\":true}");
    }
    respond(fd, 404, "text/plain", "not found");
}

}  // namespace

int runServer(int port, const std::string& root) {
    // Open a TCP socket on 127.0.0.1:<port> (this computer only, not the network).
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);
    if (bind(srv, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0 || listen(srv, 16) < 0) {
        std::perror("bind/listen");
        return 1;
    }
    std::cout << "Seam carving UI: http://localhost:" << port << "/\n";
    // Wait for connections forever (stop the program with Ctrl+C).
    for (;;) {
        int fd = accept(srv, nullptr, nullptr);
        if (fd < 0) continue;
        // One thread per request, so /api/stop is answered while a carve runs.
        std::thread([fd, root] {
            handle(fd, root);
            close(fd);
        }).detach();
    }
}
