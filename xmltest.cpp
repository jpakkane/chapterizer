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

#include <tinyxml2.h>

int main(int, char **) {
    tinyxml2::XMLDocument doc;
    auto decl = doc.NewDeclaration(nullptr);
    doc.InsertFirstChild(decl);
    auto doctype = doc.NewUnknown(
        R"(DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd")");
    doc.InsertEndChild(doctype);
    auto html = doc.NewElement("html");
    doc.InsertEndChild(html);
    html->SetAttribute("xmlns", "http://www.w3.org/1999/xhtml");
    html->SetAttribute("xml:lang", "en");

    auto head = doc.NewElement("head");
    html->InsertEndChild(head);
    auto meta = doc.NewElement("meta");
    head->InsertEndChild(meta);
    meta->SetAttribute("http-equiv", "Content-Type");
    meta->SetAttribute("content", "application/xhtml+xml; charset=utf-8");
    auto title = doc.NewElement("title");
    head->InsertEndChild(title);
    title->SetText("Name of thing");
    auto body = doc.NewElement("body");
    html->InsertEndChild(body);
    auto heading = doc.NewElement("h2");
    body->InsertEndChild(heading);
    heading->SetText("Chapter heading");
    auto p = doc.NewElement("p");
    auto text = doc.NewText("Flubbigs.");
    auto it = doc.NewElement("i");
    it->SetText("Some italic text");
    auto b = doc.NewElement("b");
    b->SetText("BOLD");
    it->InsertEndChild(b);
    p->InsertEndChild(text);
    p->InsertEndChild(it);
    body->InsertEndChild(p);
    doc.SaveFile("xmltest.html");
    return 0;
}
