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
// Real content (text lines, paragraph spacing, section padding) has
// inter-line/inter-word/inter-section whitespace that shows zero motion
// between frames even inside a genuinely scrolling region. Without closing
// these gaps, the unanimous 3-step row/col intersection fragments into short
// runs. Originally 24, sized only for Notepad's line spacing (observed:
// hundreds of individually-qualifying rows collapsed to a 10-row best run).
// That was too small for real web content: a base44.com editor preview pane
// showed bounded (real-content-on-both-sides) row gaps up to 147px between
// page sections, which clipped the detected region well above the page's
// actual bottom. Raised to 200 to cover that with margin. Gaps up to this
// length, bounded by qualifying rows/cols on both sides, are bridged before
// taking the longest contiguous run.
constexpr int PROBE_MAX_GAP_PX = 200;
// Grow-from-core column threshold. The strict PROBE_LINE_FRACTION bar finds a
// confident core of columns, but columns near a region's left/right edge are
// often background-dominant (a page's gradient margin) and carry real but
// weak motion, so they fall short of it and the region is reported narrower
// than the pane actually is. After the core is found, columns are appended
// outward from it while they clear this lower bar in EVERY probe step;
// unanimity is what still rejects one-shot animations during growth.
// Measured on a base44.com editor preview (band height 1248, per-step changed
// pixels per column): the non-scrolling sidebar peaked at 18, while the
// region's own weak right-edge columns bottomed out at 49. That is only a
// ~2.7x separation, so this sits near the geometric midpoint (~30px) rather
// than hugging either side. Growth is anchored to the strict core and halts
// at the first column that fails, so a too-low value widens the region
// rather than relocating it.
constexpr float PROBE_GROW_LINE_FRACTION = 0.024f;
constexpr int PROBE_GROW_MIN_PX = 8;
constexpr int SCROLL_TO_TOP_MAX_ITERS = 400;

} // namespace fsic
