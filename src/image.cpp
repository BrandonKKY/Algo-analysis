#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include "image.hpp"

#include <cstring>

bool loadImage(const std::string& path, Image& out, std::string& err) {
    int w, h, n;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 3);
    if (!data) {
        err = stbi_failure_reason() ? stbi_failure_reason() : "unknown error";
        return false;
    }
    out = Image(w, h);
    std::memcpy(out.px.data(), data, out.px.size());
    stbi_image_free(data);
    return true;
}

bool savePng(const std::string& path, const Image& img) {
    return stbi_write_png(path.c_str(), img.w, img.h, 3, img.px.data(), img.w * 3) != 0;
}

Image transpose(const Image& img) {
    Image t(img.h, img.w);
    for (int r = 0; r < img.h; ++r)
        for (int c = 0; c < img.w; ++c) std::memcpy(t.at(c, r), img.at(r, c), 3);
    return t;
}
