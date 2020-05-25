#/bin/sh
rm -rf docs
doxygen doxyfile
cp -r Fling-Engine-logo docs 
cd docs/html
mv * ..
rm -rf html