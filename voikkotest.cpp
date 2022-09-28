#include <libvoikko/voikko.h>
#include <cstdio>

int main(int argc, char **argv) {
    if(argc != 2) {
        printf("%s <word>\n", argv[0]);
        return 1;
    }
    const char *error;
    VoikkoHandle *vh = voikkoInit(&error, "fi", nullptr);
    if(!vh) {
        printf("Voikko init failed: %s\n", error);
        return 1;
    }
    char *hyphenation = voikkoHyphenateCstr(vh, argv[1]);
    printf("%s\n%s\n", argv[1], hyphenation);
    voikkoFreeCstr(hyphenation);
    voikkoTerminate(vh);
    return 0;
}
