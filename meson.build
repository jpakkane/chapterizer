project('chapterizer', 'cpp', default_options : ['cpp_std=c++17'])

cpp = meson.get_compiler('cpp')
hyphen_dep = cpp.find_library('hyphen')
ft_dep = dependency('freetype2')
cairo_dep = dependency('cairo')
pangocairo_dep = dependency('pangocairo')
gtk4_dep = dependency('gtk4', required: false, method: 'pkg-config')
fc_dep = dependency('fontconfig')

add_project_arguments('-Wshadow', language: 'cpp')

l = static_library('chap',
    'wordhyphenator.cpp',
    'chapterbuilder.cpp',
    'textstats.cpp',
    'pdfrenderer.cpp',
    'utils.cpp',
    'fchelpers.cpp',
    dependencies: [hyphen_dep, pangocairo_dep, cairo_dep, fc_dep]
)

executable('chapterizer',
    'main.cpp',
    link_with: l,
    dependencies: [pangocairo_dep],
)

executable('fttest', 'fttest.cpp', dependencies: [ft_dep, cairo_dep])

executable('pangotest', 'pangotest.cpp', dependencies: pangocairo_dep)

executable('devtool', 'devtool.cpp', link_with: l, dependencies: pangocairo_dep)

executable('bookmaker', 'bookmaker.cpp', link_with: l, dependencies: pangocairo_dep)

if gtk4_dep.found()
    executable('guitool', 'guitool.cpp',
      link_with: l,
      dependencies: [gtk4_dep])
endif
