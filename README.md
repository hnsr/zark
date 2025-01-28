# Zark

The beginnings of a 3D OpenGL 'engine' written in C that I never finished. Integrates Lua for scripting and
configuration, freetype for text rendering, loads a couple different mesh formats and supports per-pixel shading.

## Building

### Fedora

Install autotools and other required development packages:

```shell
yum install automake
yum install libX11-devel
yum install lua-devel
yum install ftgl-devel
yum install libXrandr-devel
yum install glew-devel
yum install DevIL-devel
```

Then set correct paths in `reconf.sh` and run the script to configure the build, then run `make`

```shell
./reconf.sh
make
```