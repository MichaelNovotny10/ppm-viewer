# ppm-img-view

A minimal PPM image viewer written in C using SDL2.

## Requirements

- GCC
- SDL2

On Debian/Ubuntu:
```
sudo apt install libsdl2-dev
```

## Building

```
make build
```

## Usage

```
./viewer <image.ppm>
```

If no file is given, it defaults to `image.ppm` in the current directory.

Only supports binary PPM files (`P6`) with a max color value of 255.

The source is extensively commented, making it a good read if you're learning C or SDL2.

## License

MIT — see [LICENSE](LICENSE). You can use, modify, and distribute this freely as long as you keep the copyright notice.

## Controls

| Key / Action | Effect |
|---|---|
| Close window | Quit |
