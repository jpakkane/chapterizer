project('chapterizer', 'cpp', default_options : ['cpp_std=c++17'])

cpp = meson.get_compiler('cpp')
hyphen_dep = cpp.find_library('hyphen')
ft_dep = dependency('freetype2')
pangocairo_dep = dependency('pangocairo')

executable('chapterizer', 'main.cpp', 'wordhyphenator.cpp',
    dependencies: hyphen_dep)

executable('fttest', 'fttest.cpp', dependencies: ft_dep)

executable('pangotest', 'pangotest.cpp', dependencies: pangocairo_dep)
