project('chapterizer', 'cpp', default_options : ['cpp_std=c++17'])

cpp = meson.get_compiler('cpp')
hyphen_dep = cpp.find_library('hyphen')
ft_dep = dependency('freetype2')
cairo_dep = dependency('cairo')
pangocairo_dep = dependency('pangocairo')
gtk4_dep = dependency('gtk4')

add_project_arguments('-Wshadow', language: 'cpp')

executable('chapterizer',
    'main.cpp',
    'wordhyphenator.cpp',
    'splitter.cpp',
    'textstats.cpp',
    'pdfrenderer.cpp',
    'utils.cpp',
    dependencies: [hyphen_dep, pangocairo_dep])

executable('fttest', 'fttest.cpp', dependencies: [ft_dep, cairo_dep])

executable('pangotest', 'pangotest.cpp', dependencies: pangocairo_dep)

executable('guitool', 'guitool.cpp', dependencies: gtk4_dep)
