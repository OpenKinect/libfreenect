wget http://sourceforge.net/projects/opentk/files/opentk/opentk-1.0/2010-10-06/opentk-2010-10-06.zip/download -Odownload.zip
unzip download.zip
rm -rf download
cp opentk/Binaries/OpenTK/Release/OpenTK.dll .
cp opentk/Binaries/OpenTK/Release/OpenTK.GLControl.dll .
echo ""
echo "-------------------------------------------"
echo "All done! Good to go!"
echo ""
echo ""
