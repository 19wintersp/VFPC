# vFPC

**A EuroScope plugin for validating flight plans in the UK.**

A re-write of the [UK VFPC client](https://github.com/VFPC/VFPC), following the
same algorithms with general enhancements and cleanup.

Licensed under the GNU General Public License, version 3.

## Build instructions

First, clone the repository, ensuring that submodules are included:

```bash
git clone --recurse-submodules https://github.com/19wintersp/VFPC vfpc
```

vFPC is built using an Ubuntu/Debian [dev container](https://containers.dev/) to
ensure a reproducible build environment with the least setup required. The
easiest way to use this container is by following the documented steps with
[VS Code](https://code.visualstudio.com/). Once the container has been built
(which may take up to 15 minutes, as downloading and extracting the Windows SDK
is quite laborious), open a new shell in the container and run:

```bash
make
```

The plugin will be built and output to "out/vFPC.dll".

Building the standalone program should be done on a normal host system, with the
appropriate tooling and compilers installed. Copy a GSG "ICAO_Aircraft.txt" file
(for example, from the UK Controller Pack) into the "res/" directory, and run:

```bash
make test
```

The program will be built and output to "out/vFPC". Running it will print help
information.

## Using the plugin

vFPC is largely compatible with VFPC, including the same tag item and function,
with similar output. The commands available are different; to see them, run
`.vfpc help` in the EuroScope command line.
