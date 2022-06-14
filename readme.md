# Chapter chapter tool

This repository contains a global line splitter algorithm (like Knuth-Plass
but not really) and a GUI tool for testing its behaviour with different
fonts, chapter widths et al.

The code only works with latin text. This is because the point of the code
is to create aesthetically pleasing results rather than "correct". The
aesthetic requirements for languages that use totally different writing system
like Japanese, Arabic and Kannada are such that they would probably require
their own custom algorithms. At the very least they would need to be written
by a native speaker.

Other limitations include:

- mixed-direction languages are obviously not supported

- the GUI is very utilitarian "engineering UI", it might
cause eye bleeding in people sensitive to UI purism

- hyphenation only supports English (and in fact requires it)

- there are some weird bugs, patches welcome

- probably only works on Linux because it uses Cairo, GTK 4, Fontconfig
et al quite heavily
