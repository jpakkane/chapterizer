#include <metadata.hpp>
#include <stdexcept>

int main(int argc, char **argv) {
    if(argc != 2) {
        printf("Fail.\n");
        return 1;
    }
    Metadata m;
    try {
        m = load_book_json(argv[1]);
    } catch(const std::exception &e) {
        printf("%s\n", e.what());
        return 1;
    }
    printf("Author is: %s\n", m.author.c_str());
    printf("%d source files\n", (int)m.sources.size());

    return 0;
}
