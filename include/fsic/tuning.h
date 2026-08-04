#pragma once

namespace fsic {

constexpr int SETTLE_POLL_MS = 40;
constexpr int SETTLE_MAX_POLLS = 25;
constexpr float SETTLE_MAD_EPS = 1.2f;
constexpr int POST_SCROLL_DELAY_MS = 60;
constexpr int ROWSIG_COLUMNS = 192;
constexpr int MIN_OVERLAP_ROWS = 24;
constexpr float MIN_OVERLAP_FRACTION = 0.15f;
constexpr int STOP_OFFSET_PX = 3;
constexpr int STOP_CONFIRM_FRAMES = 2;
constexpr int MAX_FRAMES_DEFAULT = 200;
constexpr int MAX_CANVAS_HEIGHT_DEFAULT = 30000;
constexpr float MATCH_AMBIGUITY_RATIO = 0.75f;
constexpr float STATIC_ROW_EPS = 1.5f;
constexpr int MIN_STATIC_BAND_PX = 4;
constexpr int PROBE_STEPS = 3;
constexpr int PROBE_NOTCHES = 3;
constexpr int PROBE_PIXEL_EPS = 8;
constexpr float PROBE_LINE_FRACTION = 0.12f;
constexpr int MIN_REGION_W = 80;
constexpr int MIN_REGION_H = 80;
// Real content (text lines, paragraph spacing) has inter-line/inter-word
// whitespace that shows zero motion between frames even inside a genuinely
// scrolling region. Without closing these small gaps, the unanimous 3-step
// row/col intersection fragments into short runs (observed: a real Notepad
// capture with hundreds of individually-qualifying rows collapsed to a
// 10-row best run). Gaps up to this length, bounded by qualifying rows/cols
// on both sides, are bridged before taking the longest contiguous run.
constexpr int PROBE_MAX_GAP_PX = 24;
constexpr int SCROLL_TO_TOP_MAX_ITERS = 400;

} // namespace fsic
