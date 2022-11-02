# Running the prebuilt container

On the **host**, run the Docker container interactively using bash so that it can pick up the Anaconda environment. `-v` maps a local path into the container at the given path. E.g., `docker run -it -v <host_path>:<container_path> <container> bash`.

```bash
docker run -it -v $(pwd):/data jiawen0/darktable bash
```

Once you're in the container, build Darktable with `/github/darktable/build.sh`. The default build type is "release with debug info". Others include:

- `./build.sh --build-type Debug`
- `./build.sh --build-type Release`
- `./build.sh --build-type RelWithDebInfo`

Passing `-j` increases parallelism and might make it faster.

```bash
cd /github/darktable
./build.sh -j32
```

Run the CLI with

```bash
build/bin/darktable-cli
```

# Example of running the Python wrapper

If you mapped your volumes so that `/data/input.dng` exists, then you can run an example with:

```bash
cd /py
python minimal_pipe_mit5k.py </data/input.dng>
```

Alternatively, if you didn't map your paths, you can copy data into the container with `docker cp <src> <container>:<dst>`.

This demo will do three things:

1. Do a minimal raw -> srgb rendering, writing to `/tmp/minimal_pipe.tif`.
2. Run a value sweep over contrast, writing the tapped out values to `f"/tmp/contrast_{contrast}.tif"`.
3. Run a value sweep over sharpen, writing the tapped out values to `f"/tmp/sharpen_amount_{amount}.tif"`.

Each of the three "pipelines" above are configured using their corresponding functions. Each pipeline function configures which stages are active, and for each stage, what the parameters are. For each active stage, their tapouts are written to `/tmp/<stage>_{in,out}.tmp` at each run (and overwritten by subsequent executions, unless they are converted to TIF and renamed).
