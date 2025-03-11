set positional-arguments

version := "0.0.1"
binary_name := "jotunheim"

out_dir := "out"
release_dir := out_dir / "release"
debug_dir := out_dir / "debug"

release_binary := release_dir / binary_name
debug_binary := debug_dir / binary_name

input_files := "src/*.c"

cc_args := '"-DJOTUNHEIM_VERSION=\"' + version + '\""'
gdb_args := "'-ex=tui e' -ex=r"

build:
	mkdir -p {{release_dir}}
	cc {{cc_args}} -o {{release_binary}} {{input_files}}

run *args: build
	./{{release_binary}} {{args}}

build-debug:
	mkdir -p {{debug_dir}}
	cc -g -O0 -o {{debug_binary}} {{input_files}}

debug *args: build-debug
	gdb {{gdb_args}} --args ./{{debug_binary}} {{args}}
	
