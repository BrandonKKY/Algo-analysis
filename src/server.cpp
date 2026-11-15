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

#include "seam_methods.h"

namespace {

constexpr size_t kMaxBody = 64u * 1024 * 1024;

struct Request {
    std::string method, path;
    std::map<std::string, std::string> query, headers;
    std::string body;
};

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

bool readRequest(int fd, Request& req) {
    std::string buf;
    char tmp[8192];
    size_t headerEnd;
    while ((headerEnd = buf.find("\r\n\r\n")) == std::string::npos) {
        ssize_t n = recv(fd, tmp, sizeof tmp, 0);
        if (n <= 0 || buf.size() > 65536) return false;
        buf.append(tmp, n);
    }
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

void respond(int fd, int status, const std::string& type, const std::string& body) {
    const char* text = status == 200 ? "OK" : status == 400 ? "Bad Request" : status == 404 ? "Not Found"
                       : status == 501 ? "Not Implemented" : "Error";
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

std::string jsonEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        if (static_cast<unsigned char>(c) >= 0x20) o += c;
    }
    return o;
}

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

bool isImageName(const std::string& n) {
    std::string l = n;
    std::transform(l.begin(), l.end(), l.begin(), ::tolower);
    for (const char* e : {".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp"})
        if (l.size() > std::strlen(e) && l.compare(l.size() - std::strlen(e), std::strlen(e), e) == 0) return true;
    return false;
}

void serveFile(int fd, const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return respond(fd, 404, "text/plain", "not found");
    std::stringstream ss;
    ss << f.rdbuf();
    respond(fd, 200, mimeFor(path), ss.str());
}

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
    if (!carve(img, finder, count, seams, err)) return fail(501, err);

    std::string j = "{\"seams\":[";
    for (size_t i = 0; i < seams.size(); ++i) {
        j += i ? ",[" : "[";
        for (size_t y = 0; y < seams[i].size(); ++y) j += (y ? "," : "") + std::to_string(seams[i][y]);
        j += "]";
    }
    respond(fd, 200, "application/json", j + "]}");
}

void handle(int fd, const std::string& root) {
    Request req;
    if (!readRequest(fd, req)) return respond(fd, 400, "text/plain", "bad request");
    if (req.path.find("..") != std::string::npos) return respond(fd, 400, "text/plain", "bad path");

    if (req.method == "GET" && (req.path == "/" || req.path == "/index.html")) return serveFile(fd, root + "/index.html");
    if (req.method == "GET" && req.path == "/api/images") return listImages(fd, root);
    if (req.method == "GET" && req.path.rfind("/images/", 0) == 0) return serveFile(fd, root + req.path);
    if (req.method == "POST" && req.path == "/api/carve") return handleCarve(fd, req);
    respond(fd, 404, "text/plain", "not found");
}

}  // namespace

int runServer(int port, const std::string& root) {
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
    for (;;) {
        int fd = accept(srv, nullptr, nullptr);
        if (fd < 0) continue;
        handle(fd, root);
        close(fd);
    }
}
