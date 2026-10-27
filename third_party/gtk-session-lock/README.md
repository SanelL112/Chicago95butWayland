# gtk-session-lock

The custom locker links to unmodified gtk-session-lock 0.2.0, pinned to commit
b9ddb2792b613d14622acada73c64f16a2635b40 from
https://github.com/Cu3PO42/gtk-session-lock .

Build with `scripts/build-lock-dependency.sh` or install gtk-session-lock-devel.
The script retains the corresponding source in the user dependency directory.
For offline builds supply CHICAGO_SESSION_LOCK_SOURCE pointing to that pinned
source tree, with its UPSTREAM_REVISION file. The built shared library is copied
to lib/ for a relative runtime path; it is not a source-controlled artifact.

The upstream GPLv3 and MIT license texts are retained here. Some upstream GTK
private-access files also contain their original notices, preserved in the
source tree. GTK upgrades require rerunning the live lock tests because the
library uses GTK internals.
