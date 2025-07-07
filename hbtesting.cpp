// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Jussi Pakkanen

#include <vector>
#include <string>
#include <memory>
#include <cassert>
#include <hb.h>

#define HB_NUM_STEPS 64

struct HBFontCloser {
    void operator()(hb_font_t *f) { hb_font_destroy(f); }
};

struct HBBufCloser {
    void operator()(hb_buffer_t *buf) { hb_buffer_destroy(buf); }
};

struct HBFaceCloser {
    void operator()(hb_face_t *face) { hb_face_destroy(face); }
};

struct HBBlobCloser {
    void operator()(hb_blob_t *blob) { hb_blob_destroy(blob); }
};

struct TextUnit {
    std::string regular_file, italic_file;
    std::vector<std::string> features;
    double pointsize;

    bool is_smallcaps() const {
        for(const auto &f : features) {
            if(f == "smcp") {
                return true;
            }
        }
        return false;
    }
    std::unique_ptr<hb_font_t, HBFontCloser> regular;
    std::unique_ptr<hb_font_t, HBFontCloser> italic;
};

std::unique_ptr<hb_font_t, HBFontCloser> load_font(const std::string &f, const double pointsize) {
    const double hbscale = pointsize * HB_NUM_STEPS;
    std::unique_ptr<hb_blob_t, HBBlobCloser> b(hb_blob_create_from_file(f.c_str()));
    std::unique_ptr<hb_face_t, HBFaceCloser> face(hb_face_create(b.get(), 0));
    std::unique_ptr<hb_font_t, HBFontCloser> font(hb_font_create(face.get()));
    hb_font_set_scale(font.get(), hbscale, hbscale);
    return font;
}

std::unique_ptr<hb_buffer_t, HBBufCloser> render_text(const char *txt, const TextUnit &tu) {
    const bool is_italic = false;
    const bool is_smallcaps = false;
    hb_font_t *font = is_italic ? tu.italic.get() : tu.regular.get();
    std::vector<hb_feature_t> joined_features;
    for(const auto &feat : tu.features) {
        hb_feature_t f;
        assert(feat.size() == 4);
        f.tag = HB_TAG(feat[0], feat[1], feat[2], feat[3]);
        f.value = 1;
        f.start = HB_FEATURE_GLOBAL_START;
        f.end = HB_FEATURE_GLOBAL_END;
        joined_features.push_back(std::move(f));
    }
    if(is_smallcaps) {
        if(tu.is_smallcaps()) {
            fprintf(stderr, "Using small caps in already smallcaps text.\n");
            std::abort();
        }
        hb_feature_t f;
        f.tag = HB_TAG('s', 'm', 'c', 'p');
        f.value = 1;
        f.start = HB_FEATURE_GLOBAL_START;
        f.end = HB_FEATURE_GLOBAL_END;
        joined_features.push_back(std::move(f));
    }
    std::unique_ptr<hb_buffer_t, HBBufCloser> buf(hb_buffer_create());
    hb_buffer_add_utf8(buf.get(), txt, -1, 0, -1);
    hb_buffer_set_direction(buf.get(), HB_DIRECTION_LTR);
    hb_buffer_set_script(buf.get(), HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf.get(), hb_language_from_string("en", -1));

    hb_buffer_guess_segment_properties(buf.get());

    if(joined_features.empty()) {
        hb_shape(font, buf.get(), nullptr, 0);
    } else {
        hb_shape(font, buf.get(), joined_features.data(), joined_features.size());
    }
    return buf;
}

void load_fonts(TextUnit &tu) {
    if(!tu.regular_file.empty()) {
        tu.regular = load_font(tu.regular_file, tu.pointsize);
    }
    if(!tu.italic_file.empty()) {
        tu.italic = load_font(tu.italic_file, tu.pointsize);
    }
}

int main(int, char **) { return 0; }
