# Book PDF creator

This repository contains code to create a print-ready PDF and epub
versions of a book written in a custom markdownish syntax called
_bookdown_.

The core algorithm is a global line splitter algorithm (a fairly
heavily tweaked Knuth-Plass). There is also a GUI tool for testing its
behaviour with different fonts, chapter widths et al.

The code only works with latin text. This is because the point of the
code is to create aesthetically pleasing results rather than "correct"
ones. The aesthetic requirements for languages that use a different
writing system like CJK, Arabic and Kannada are such that they would
probably require their own custom algorithms. This might even be the
case for "latin-like" scripts like Greek. At the very least they would
need to be written by a native speaker.

Other limitations include:

- mixed-direction texts are obviously not supported

- the GUI is very utilitarian "engineering UI", it might
cause eye bleeding in people sensitive to UI purism

- hyphenation only supports English and Finnish

- there are some weird bugs, patches welcome

- probably only works on Linux because it uses Cairo, GTK 4,
Fontconfig et al quite heavily

- some visual aspects are defined in the code itself rather than the
JSON conf file as the current main use case is to generate my own
books rather than provide a fully general tool

## Building and using

Add dependencies with:

```
sudo apt install libhyphen-dev libgtk-4-dev libtinyxml2-dev libvoikko-dev
```

Building is done in the standard Meson way:

```
<do a git checkout and cd into it>
mkdir builddir
cd builddir
meson ..
ninja
```

Then run either the GUI tool:

```
./guitool
```

Or process a book json:

```
./bookmaker ../testdoc/sample.json
```
