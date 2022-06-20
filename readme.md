# Visual chapter linebreaking tool

This repository contains a global line splitter algorithm (like
Knuth-Plass but not really) and a GUI tool for testing its behaviour
with different fonts, chapter widths et al.

The code only works with latin text. This is because the point of the
code is to create aesthetically pleasing results rather than
"correct". The aesthetic requirements for languages that use totally
different writing system like CJK, Arabic and Kannada are such that
they would probably require their own custom algorithms. This might
even be the case for "latin-like" scripts like Greek. At the very
least they would need to be written by a native speaker.

Other limitations include:

- mixed-direction texts are obviously not supported

- the GUI is very utilitarian "engineering UI", it might
cause eye bleeding in people sensitive to UI purism

- hyphenation only supports English (and in fact requires it)

- there are some weird bugs, patches welcome

- probably only works on Linux because it uses Cairo, GTK 4, Fontconfig
et al quite heavily

## Building and using

The program depends on GTK 4 and libhyphen. On debianlike distros they
can be installed with:

```
sudo apt install libhyphen-dev libgtk-4-dev
```

Building is done in the standard Meson way:

```
<do a git checkout and cd into it>
mkdir builddir
cd builddir
meson ..
ninja
./guitool
```
