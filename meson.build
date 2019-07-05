project('gterm', 'c', default_options : [ 'c_std=c99' ] )

# tweak warnings
add_global_arguments('-D_POSIX_SOURCE=1', language : 'c')
add_global_arguments('-D_GNU_SOURCE=1', language : 'c')

# pkg-config deps
gtk_dep = dependency('gtk+-3.0', method: 'pkg-config')
vte_dep = dependency('vte-2.91', method: 'pkg-config')

executable('gterm',
           sources      : [ 'gterm.c' ],
           dependencies : [ gtk_dep, vte_dep ],
           install      : true)
install_man('gterm.1')
install_data('gterm.desktop',
	     install_dir : 'share/applications')

executable('gload',
           sources      : [ 'gload.c' ],
           dependencies : [ gtk_dep ],
           install      : true)
install_man('gload.1')
