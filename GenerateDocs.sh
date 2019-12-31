#/bin/sh
doxygen doxyfile
cd docs/html
mv * ..
rm -rf html
