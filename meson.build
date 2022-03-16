project('chapterizer', 'cpp', default_options : ['cpp_std=c++17'])

cpp = meson.get_compiler('cpp')
hyphen_dep = cpp.find_library('hyphen')
ft_dep = dependency('freetype2')

executable('chapterizer', 'main.cpp', 'wordhyphenator.cpp',
    dependencies: hyphen_dep)

executable('fttest', 'fttest.cpp', dependencies: ft_dep)
