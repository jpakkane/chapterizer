project('chapterizer', 'cpp', default_options : ['cpp_std=c++20'])

cpp = meson.get_compiler('cpp')
hyphen_dep = cpp.find_library('hyphen')
ft_dep = dependency('freetype2')
cairo_dep = dependency('cairo')
pangocairo_dep = dependency('pangocairo')
gtk4_dep = dependency('gtk4', required: false, method: 'pkg-config')
fc_dep = dependency('fontconfig')
glib_dep = dependency('glib-2.0')
tixml_dep = dependency('tinyxml2')
nljson_dep = dependency('nlohmann_json')
voikko_dep = dependency('libvoikko')

add_project_arguments('-Wshadow', language: 'cpp')

l = static_library('chap',
    'wordhyphenator.cpp',
    'paragraphformatter.cpp',
    'textstats.cpp',
    'pdfrenderer.cpp',
    'utils.cpp',
    'fchelpers.cpp',
    'formatting.cpp',
    'bookparser.cpp',
    'epub.cpp',
    'paginator.cpp',
    'metadata.cpp',
    dependencies: [hyphen_dep, pangocairo_dep, cairo_dep, fc_dep, voikko_dep]
)

executable('pangotest', 'pangotest.cpp', dependencies: pangocairo_dep)

executable('bookmaker', 'bookmaker.cpp',
    link_with: l,
    dependencies: [pangocairo_dep, tixml_dep])

if gtk4_dep.found()
    executable('guitool', 'guitool.cpp',
      link_with: l,
      dependencies: [gtk4_dep])
endif

executable('tests', 'tests.cpp',
    link_with: [l],
    dependencies: glib_dep)

executable('mdtool', 'mdtool.cpp',
    link_with: l)

executable('voikkotest', 'voikkotest.cpp',
    dependencies: voikko_dep)

executable('blocktest', 'blocktest.cpp')

executable('paginationtest', 'paginationtest.cpp',
    dependencies: cairo_dep)
