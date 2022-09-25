#pragma once

#include <bookparser.hpp>
#include <pdfrenderer.hpp>

class Paginator {
public:
    Paginator(const Document &d);

    void generate_pdf(const char *outfile);

private:
    const Document &doc;
};
