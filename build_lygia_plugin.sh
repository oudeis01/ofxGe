#!/usr/bin/bash


# clean the old plugin
rm -rf plugins;

rm -rf bin/data/plugins/lygia;
echo "Cleaning old plugins...";
sleep 2;



cd glsl-plugin-dev-kit;

python3 generate_plugin.py lygia ./lygia ../plugins/lygia-plugin --version 0.0.1 --author 'Unknown' --max-functions-per-file 50;

cd ../plugins/lygia-plugin;
ln -s ../../glsl-plugin-interface .;
mkdir -p build;
cd build;
cmake -DCMAKE_BUILD_TYPE=DEBUG ..;
make -j$(nproc);

# copy it to the plugins directory
cd ../../../;
git clone https://github.com/patriciogonzalezvivo/lygia.git bin/data/plugins/lygia --depth=1;
cp plugins/lygia-plugin/build/*.so bin/data/plugins/lygia/;


