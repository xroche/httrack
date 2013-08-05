cd httrack/trunk
rm -rf zip_resources resources.zip
mkdir zip_resources
rsync -av --exclude ".svn" --exclude "*~" --exclude "*/server/*" templates html zip_resources/
cd zip_resources
zip -r ../resources.zip templates html
cd ..
rm -rf zip_resources
