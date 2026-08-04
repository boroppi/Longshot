#pragma once
#include <string>
#include <vector>
#include "fsic/backend.h"

namespace fsic {

// A complete, non-stub backend that replays a pre-rendered sequence of
// frame_*.bmp images from disk instead of touching any real screen or input
// device. Used so the core pipeline (probe, session, stitch) is fully
// testable without a live window.
class SyntheticBackend final : public IBackend {
public:
    explicit SyntheticBackend(std::string dir);
    ~SyntheticBackend() override = default;

    const char* name() const override { return "synthetic"; }
    Status init() override;
    void shutdown() override {}

    Status check_permissions(PermissionStatus* out) override;
    Status request_permissions() override;

    Status enumerate_windows(std::vector<WindowInfo>* out) override;
    Status window_at_point(Point screen_px, WindowInfo* out) override;
    Status get_window_bounds(uint64_t window_id, Rect* frame_px, Rect* client_px) override;
    Status activate_window(uint64_t window_id) override;

    Status get_virtual_desktop_bounds(Rect* out_px) override;
    double get_scale_for_point(Point screen_px) override;

    Status capture_rect(const Rect& rect_px, Image* out) override;

    Status get_cursor_position(Point* out_px) override;
    Status move_cursor(Point screen_px) override;
    Status inject_scroll(Point screen_px, ScrollDir dir, int notches) override;
    Status inject_key(uint64_t window_id, Key key) override;

    void sleep_ms(int ms) override;

    // Test-only extension (not part of IBackend): registers independently
    // scrollable sub-regions. inject_scroll advances whichever registered
    // track's rect contains the point; capture_rect composites each track's
    // current frame on top of the fallback frame before cropping. With zero
    // tracks registered, behavior is identical to the plain single-track
    // backend used by real capture/list/stitch flows.
    void set_tracks(std::vector<Rect> region_rects, std::vector<std::vector<Image>> track_frames);

private:
    std::string dir_;
    std::vector<Image> frames_;
    size_t index_ = 0;
    Point cursor_{0, 0};

    std::vector<Rect> track_rects_;
    std::vector<std::vector<Image>> track_frames_;
    std::vector<size_t> track_index_;

    static constexpr uint64_t kWindowId = 1;
};

} // namespace fsic
