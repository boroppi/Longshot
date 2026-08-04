#pragma once
#include <string>
#include "fsic/status.h"
#include "fsic/image.h"

namespace fsic {
Status write_bmp(const std::string& path, const Image& img);
Status read_bmp(const std::string& path, Image* out);
Status write_png(const std::string& path, const Image& img);
Status write_image_auto(const std::string& path, const Image& img);
} // namespace fsic
