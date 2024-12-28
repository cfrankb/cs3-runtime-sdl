# CS3 Runtime SDL

This project aims to port to the CS3 Runtime to a wide variety of platforms. 

Using the SDL2 libraries as a base, the game can now run as a desktop app or in the browser.


![alt text](images/Screenshot_2023-12-20_04-29-08.png)



## Building the runtime

### Online version


The online version requires SDL2, zlib and Emscripten.


<b> Build cs3 runtime</b>

First install emscripten : https://emscripten.org/index.html

Run these commands
```
$ python bin/gen.py emsdl
$ emmake make
```

<b>Launch the application</b>


```
$ emrun build/cs3v2.html
```

### Map Editor

https://github.com/cfrankb/cs3-map-edit

### Play online

https://cfrankb.itch.io/creepspread-iii

