project('chapterizer', 'cpp', default_options : ['cpp_std=c++17'])

cpp = meson.get_compiler('cpp')
hyphen_dep = cpp.find_library('hyphen')

executable('chapterizer', 'main.cpp', 'wordhyphenator.cpp',
    dependencies: hyphen_dep)
