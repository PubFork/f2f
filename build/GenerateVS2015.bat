set CMAKE=cmake

%CMAKE% -version
%CMAKE% -G  "Visual Studio 14 2015" -D BOOST_ROOT=d:\Work\boost_1_60_0 %~dp0..