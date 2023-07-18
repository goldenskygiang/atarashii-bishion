# 新しいビシオン！

Another プロジェクト for the 11th Cloud Programming World Cup in 2023.

今年は、ペンシルバニアから東京まで行きましょう！
もう一度，日本へ戻ってくる！

## Get started
- Install Visual Studio 2022.
- Download OpenCV.

### Build and install dlib
- Download [dlib](http://dlib.net/) then extract the archive.
- Create a `build` folder in the parent folder of `dlib, docs, examples`, etc.
- Type the following commands to build `dlib`.
```
cd build

cmake -G "Visual Studio 17 2022" `
-DJPEG_INCLUDE_DIR=..\dlib\external\libjpeg `
-DJPEG_LIBRARY=..\dlib\external\libjpeg `
-DPNG_PNG_INCLUDE_DIR=..\dlib\external\libpng `
-DPNG_LIBRARY_RELEASE=..\dlib\external\libpng `
-DZLIB_INCLUDE_DIR=..\dlib\external\zlib `
-DZLIB_LIBRARY_RELEASE=..\dlib\external\zlib `
-DCMAKE_INSTALL_PREFIX=install_debug ..
 
cmake --build . --config Debug --target INSTALL

cmake -G "Visual Studio 17 2022" `
-DJPEG_INCLUDE_DIR=..\dlib\external\libjpeg `
-DJPEG_LIBRARY=..\dlib\external\libjpeg `
-DPNG_PNG_INCLUDE_DIR=..\dlib\external\libpng `
-DPNG_LIBRARY_RELEASE=..\dlib\external\libpng `
-DZLIB_INCLUDE_DIR=..\dlib\external\zlib `
-DZLIB_LIBRARY_RELEASE=..\dlib\external\zlib `
-DCMAKE_INSTALL_PREFIX=install_release ..

cmake --build . --config Release --target INSTALL
```
