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

// Need to test something quickly? Edit this file.

#include <textstats.hpp>
#include <cstdio>

const char *text =
    "From the corner of the divan of Persian saddle-bags on which he was lying, smoking, as";

int main(int, char **) {
    TextStats s;
    FontParameters p;
    p.name = "Gentium";
    p.point_size = 10;
    p.type = FontStyle::Regular;
    printf("Regular width: %.2f\n", s.text_width(text, p));
    p.type = FontStyle::Italic;
    printf("Italic width: %.2f\n", s.text_width(text, p));
    return 0;
}
