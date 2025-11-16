project('chapterizer', 'cpp', default_options : ['cpp_std=c++20', 'warning_level=2'])

cpp = meson.get_compiler('cpp')
hyphen_dep = cpp.find_library('hyphen')
ft_dep = dependency('freetype2')
gtk4_dep = dependency('gtk4', required: false, method: 'pkg-config')
fc_dep = dependency('fontconfig')
glib_dep = dependency('glib-2.0')
tixml_dep = dependency('tinyxml2')
nljson_dep = dependency('nlohmann_json')
voikko_dep = dependency('libvoikko')
hb_dep = dependency('harfbuzz')
capy_dep = dependency('capypdf')

add_project_arguments('-Wshadow', language: 'cpp')

l = static_library('chap',
    'wordhyphenator.cpp',
    'paragraphformatter.cpp',
    'draftparagraphformatter.cpp',
    'hbmeasurer.cpp',
    'capypdfrenderer.cpp',
    'utils.cpp',
    'fchelpers.cpp',
    'formatting.cpp',
    'bookparser.cpp',
    'epub.cpp',
    'draftpaginator.cpp',
    'printpaginator.cpp',
    'chapterformatter.cpp',
    'metadata.cpp',
    'hbfontcache.cpp',
    dependencies: [hyphen_dep, fc_dep, glib_dep, voikko_dep, hb_dep, capy_dep]
)

executable('bookmaker', 'bookmaker.cpp',
    link_with: l,
    dependencies: [tixml_dep, hb_dep, capy_dep])

#if gtk4_dep.found()
#    executable('guitool', 'guitool.cpp',
#      link_with: l,
#      dependencies: [gtk4_dep])
#endif

executable('tests', 'tests.cpp',
    link_with: [l],
    dependencies: glib_dep)

executable('mdtool', 'mdtool.cpp',
    link_with: l)

executable('voikkotest', 'voikkotest.cpp',
    dependencies: voikko_dep)

executable('blocktest', 'blocktest.cpp')

executable('hbtest', 'hbtest.cpp', 'hbfontcache.cpp',
    dependencies: [ft_dep, hb_dep])
