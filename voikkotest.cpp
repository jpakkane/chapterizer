/*
 * Copyright 2022 Jussi Pakkanen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
