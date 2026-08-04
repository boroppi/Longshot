#include "fsic/rowsig.h"

#include <cstdio>

#include "fsic/image.h"
#include "test_util.h"

using namespace fsic;

namespace {

Image make_solid(int w, int h, uint8_t gray) {
    Image img;
    img.resize(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t* p = img.row(y) + static_cast<size_t>(x) * 4;
            p[0] = p[1] = p[2] = gray;
            p[3] = 255;
        }
    }
    return img;
}

void test_constant_rows() {
    Image img = make_solid(64, 32, 128);
    RowSignatures sig = compute_row_signatures(img, 192);
    for (int y = 1; y < sig.rows; ++y) {
        for (int i = 0; i < sig.cols; ++i) {
            FSIC_CHECK_EQ(sig.row(0)[i], sig.row(y)[i]);
        }
    }
}

void test_self_distance_zero() {
    Image img = make_solid(40, 20, 77);
    RowSignatures sig = compute_row_signatures(img, 192);
    FSIC_CHECK_NEAR(row_distance(sig, 5, sig, 5), 0.0f, 1e-6);
}

void test_distance_grows_with_gap() {
    Image base = make_solid(40, 10, 100);
    Image near = make_solid(40, 10, 110);
    Image far = make_solid(40, 10, 150);

    RowSignatures sb = compute_row_signatures(base, 192);
    RowSignatures sn = compute_row_signatures(near, 192);
    RowSignatures sf = compute_row_signatures(far, 192);

    float d_near = row_distance(sb, 0, sn, 0);
    float d_far = row_distance(sb, 0, sf, 0);

    FSIC_CHECK(d_near > 0.0f);
    FSIC_CHECK(d_far > d_near);
}

void test_cols_clamp() {
    Image img = make_solid(5, 5, 200);
    RowSignatures sig = compute_row_signatures(img, 192);
    FSIC_CHECK_EQ(sig.cols, 5);
}

} // namespace

int main() {
    test_constant_rows();
    test_self_distance_zero();
    test_distance_grows_with_gap();
    test_cols_clamp();
    if (g_fsic_test_failures == 0) std::printf("test_rowsig: all checks passed\n");
    FSIC_TEST_MAIN_END
}
