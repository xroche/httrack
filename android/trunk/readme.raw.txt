cd httrack/trunk
rm -rf zip_resources resources.zip
mkdir zip_resources
rsync -av --exclude ".svn" --exclude "*~" --exclude "*/server/*" --exclude "Makefile.*" --exclude "*.sh" templates html zip_resources/
cd zip_resources
mkdir license
cd license
wget http://www.gnu.org/licenses/gpl-3.0-standalone.html
cd ..
zip -r ../resources.zip templates html license
cd ..
rm -rf zip_resources
