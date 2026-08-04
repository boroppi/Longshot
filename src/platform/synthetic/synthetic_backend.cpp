#include "synthetic_backend.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <utility>

#include "fsic/image_io.h"

namespace fsic {

namespace fs = std::filesystem;

SyntheticBackend::SyntheticBackend(std::string dir) : dir_(std::move(dir)) {}

Status SyntheticBackend::init() {
    frames_.clear();
    for (int i = 0;; ++i) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "frame_%03d.bmp", i);
        const fs::path p = fs::path(dir_) / buf;
        if (!fs::exists(p)) break;
        Image img;
        Status st = read_bmp(p.string(), &img);
        if (!st) return st;
        frames_.push_back(std::move(img));
    }
    if (frames_.empty()) {
        return Status::Error("synthetic backend: no frame_*.bmp files found in " + dir_);
    }
    index_ = 0;
    cursor_ = Point{frames_[0].w / 2, frames_[0].h / 2};
    return Status::Ok();
}

Status SyntheticBackend::check_permissions(PermissionStatus* out) {
    *out = PermissionStatus{true, true, ""};
    return Status::Ok();
}

Status SyntheticBackend::request_permissions() { return Status::Ok(); }

Status SyntheticBackend::enumerate_windows(std::vector<WindowInfo>* out) {
    out->clear();
    WindowInfo w;
    w.id = kWindowId;
    w.title = "synthetic";
    w.app_name = "longshot_synthetic";
    w.frame_px = Rect{0, 0, frames_[0].w, frames_[0].h};
    w.client_px = w.frame_px;
    w.scale = 1.0;
    w.on_screen = true;
    out->push_back(w);
    return Status::Ok();
}

Status SyntheticBackend::window_at_point(Point screen_px, WindowInfo* out) {
    const Rect bounds{0, 0, frames_[0].w, frames_[0].h};
    if (!bounds.contains(screen_px)) return Status::Error("synthetic: no window at point");
    std::vector<WindowInfo> all;
    enumerate_windows(&all);
    *out = all[0];
    return Status::Ok();
}

Status SyntheticBackend::get_window_bounds(uint64_t window_id, Rect* frame_px, Rect* client_px) {
    if (window_id != kWindowId) return Status::Error("synthetic: unknown window id");
    *frame_px = Rect{0, 0, frames_[0].w, frames_[0].h};
    *client_px = *frame_px;
    return Status::Ok();
}

Status SyntheticBackend::activate_window(uint64_t window_id) {
    if (window_id != kWindowId) return Status::Error("synthetic: unknown window id");
    return Status::Ok();
}

Status SyntheticBackend::get_virtual_desktop_bounds(Rect* out_px) {
    *out_px = Rect{0, 0, frames_[0].w, frames_[0].h};
    return Status::Ok();
}

double SyntheticBackend::get_scale_for_point(Point) { return 1.0; }

Status SyntheticBackend::capture_rect(const Rect& rect_px, Image* out) {
    if (track_rects_.empty()) {
        *out = crop(frames_[index_], rect_px);
        return Status::Ok();
    }
    Image composite = frames_[index_];
    for (size_t t = 0; t < track_rects_.size(); ++t) {
        const auto& track = track_frames_[t];
        const size_t idx = std::min(track.size() - 1, track_index_[t]);
        const Rect& r = track_rects_[t];
        blit(composite, r.x, r.y, track[idx], Rect{0, 0, r.w, r.h});
    }
    *out = crop(composite, rect_px);
    return Status::Ok();
}

Status SyntheticBackend::get_cursor_position(Point* out_px) {
    *out_px = cursor_;
    return Status::Ok();
}

Status SyntheticBackend::move_cursor(Point screen_px) {
    cursor_ = screen_px;
    return Status::Ok();
}

Status SyntheticBackend::inject_scroll(Point screen_px, ScrollDir dir, int notches) {
    cursor_ = screen_px;

    for (size_t t = 0; t < track_rects_.size(); ++t) {
        if (!track_rects_[t].contains(screen_px)) continue;
        size_t& idx = track_index_[t];
        const size_t n = track_frames_[t].size();
        if (dir == ScrollDir::Down) {
            const size_t advance = static_cast<size_t>(std::max(0, notches));
            idx = std::min(n - 1, idx + advance);
        } else {
            const size_t retreat = static_cast<size_t>(std::max(0, notches));
            idx = (retreat > idx) ? 0 : idx - retreat;
        }
        return Status::Ok();
    }

    if (dir == ScrollDir::Down) {
        const size_t advance = static_cast<size_t>(std::max(0, notches));
        index_ = std::min(frames_.size() - 1, index_ + advance);
    } else {
        const size_t retreat = static_cast<size_t>(std::max(0, notches));
        index_ = (retreat > index_) ? 0 : index_ - retreat;
    }
    return Status::Ok();
}

void SyntheticBackend::set_tracks(std::vector<Rect> region_rects,
                                   std::vector<std::vector<Image>> track_frames) {
    track_rects_ = std::move(region_rects);
    track_frames_ = std::move(track_frames);
    track_index_.assign(track_rects_.size(), 0);
}

Status SyntheticBackend::inject_key(uint64_t window_id, Key key) {
    if (window_id != kWindowId) return Status::Error("synthetic: unknown window id");
    switch (key) {
        case Key::PageDown:
            index_ = std::min(frames_.size() - 1, index_ + 3);
            break;
        case Key::PageUp:
            index_ = (3 > index_) ? 0 : index_ - 3;
            break;
        case Key::Home:
            index_ = 0;
            break;
        case Key::End:
            index_ = frames_.size() - 1;
            break;
    }
    return Status::Ok();
}

void SyntheticBackend::sleep_ms(int) {
    // No-op: tests run instantly and deterministically.
}

} // namespace fsic
