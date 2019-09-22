#/bin/sh
doxygen doxyfile
cd docs/html
cp -rp * ..
