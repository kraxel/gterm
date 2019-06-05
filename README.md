[![Travis Status](https://travis-ci.com/kraxel/gterm.svg?branch=master)](https://travis-ci.com/kraxel/gterm)
[![Copr Status](https://copr.fedorainfracloud.org/coprs/kraxel/mine.git/package/gterm/status_image/last_build.png)](https://copr.fedorainfracloud.org/coprs/kraxel/mine.git/package/gterm/)

# gterm

Terminal application, based on gtk3 and vte.  The plan is to have a
modern terminal (which can -- for example -- run on wayland and render
emoji) for xterm fans.

Command line options are compatible with xterm.  Likewise config file
key naming follows xterm resource naming.  No config UI, you have to
edit the ~/.config/gterm.conf config file, simliar to editing
~/.Xdefaults for xterm.

The number of supported config options is rather small right now, but
is expected to grow over time to cover the most important ones.  It
will probably never fully match the xterm feature set though.  Quite a
few xterm features are pretty much obsolete these days and/or will simply
not work with wayland.  The x11 server side font rendering comes to mind
for example.  Also any charset quirks dating back to the early x11 days,
before unicode and utf-8 did exist.
