# Jotunheim

A simple partially implemented language based on the syntax of Odin and Jai using the QBE backend.

## Build from source

```bash
just build
```

The resulting binary is found at `out/release/jotunheim`

## Usage

Compile a file: input.jh

```bash
jotunheim <input.jh>
```

## Examples

### Hello world

```jotunheim
printf :: proc();

main :: proc () {
  printf("Hello, world!\n");

  return 0;
}
```
