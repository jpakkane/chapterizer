#pragma once

#include <bookparser.hpp>

class Epub {
public:
    explicit Epub(Document &d);

    void generate(const char *ofilename);

private:
    Document &doc;
};
