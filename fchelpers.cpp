#include "fchelpers.hpp"
#include <fontconfig/fontconfig.h>
#include <set>
#include <cstring>
#include <array>

namespace {

const FcChar8 tmpl[] = "%{family[0]}";

bool startswith(const char *string_to_search, const char *to_search) {
    return strncmp(string_to_search, to_search, strlen(to_search)) == 0;
}

const std::array<const char *, 10> wanted_words{
    "Gentium",
    "Liberation",
    "DejaVu",
    "Nimbus",
    "URW ",
    "Ubuntu",
    "P05",
    "C059",
    "Hofshi",
    "IBM",
};

// The default Linux install has a gazillion fonts that makes all font
// lists unreadably large. Pick only a reasonable subset here. Feel
// free to change this to add more useful fonts.
//
// This app only deals with roman text, so any font aimed at languages
// like japanese, arabic, indian scripts etc need to be removed because
// they would never make sense in the font list.

bool is_bad(char *fontname) {
    if(startswith(fontname, "Noto ")) {
        if(strcmp(fontname, "Noto Sans") == 0) {
            return false;
        }
        if(strcmp(fontname, "Noto Serif") == 0) {
            return false;
        }
        return true;
    }
    if(strstr(fontname, "TeX")) {
        return true;
    }
    for(const char *w : wanted_words) {
        if(strstr(fontname, w) != 0) {
            return false;
        }
    }
    return true;
}

} // namespace

std::vector<std::string> get_fontnames_smart() {
    std::set<std::string> store;
    std::vector<std::string> fontnames;
    FcPattern *pattern = FcPatternCreate();
    FcObjectSet *oset = FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_FILE, (char *)0);

    FcFontSet *fset = FcFontList(0, pattern, oset);

    for(int i = 0; i < fset->nfont; ++i) {
        FcChar8 *s = FcPatternFormat(fset->fonts[i], tmpl);
        if(!is_bad((char *)s)) {
            store.insert((char *)s);
        }
        FcStrFree(s);
    }

    for(const auto &s : store) {
        fontnames.push_back(s);
        printf("%s\n", s.c_str());
    }
    printf("Found %d fonts.\n", (int)fontnames.size());
    FcFontSetDestroy(fset);
    FcObjectSetDestroy(oset);
    FcPatternDestroy(pattern);
    return fontnames;
}
