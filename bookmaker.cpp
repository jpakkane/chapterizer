#include <paginator.hpp>
#include <epub.hpp>
#include <utils.hpp>

#include <glib.h>
#include <cassert>
#include <cstring>

Document load_document(const char *fname) {
    Document doc;
    MMapper map(fname);
    if(!g_utf8_validate(map.data(), map.size(), nullptr)) {
        printf("Invalid utf-8.\n");
        std::abort();
    }
    GError *err = nullptr;
    if(err) {
        std::abort();
    }

    LineParser linep(map.data(), map.size());
    StructureParser strucp;

    line_token token = linep.next();
    while(!std::holds_alternative<EndOfFile>(token)) {
        strucp.push(token);
        token = linep.next();
    }
    strucp.push(token);

    // FIXME, add stored data if any.
    return strucp.get_document();
}

int main(int argc, char **argv) {
    if(argc != 2) {
        printf("%s <input text file>\n", argv[0]);
        return 1;
    }
    auto doc = load_document(argv[1]);
    // Paginator p(doc);
    // p.generate_pdf("bookout.pdf");
    Epub epub(doc);
    epub.generate("epub_test.epub");
    return 0;
}
