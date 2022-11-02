FROM ubuntu:20.04

RUN apt-get update \
  && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
  gcc g++ cmake intltool xsltproc libgtk-3-dev libxml2-utils libxml2-dev liblensfun-dev librsvg2-dev libsqlite3-dev libcurl4-gnutls-dev libjpeg-dev libtiff5-dev liblcms2-dev libjson-glib-dev libexiv2-dev libpugixml-dev \
  git gdb

WORKDIR github

RUN git clone --recurse-submodules --depth 1 https://github.com/darktable-org/darktable.git

WORKDIR darktable

RUN git fetch --tags \
  && git checkout tags/release-3.8.1 \
  && git submodule update --init \
  && git switch -c diff_proxy

# Install the Anaconda toolchain for Python.
RUN apt-get install wget
RUN cd /tmp \
  && wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh \
  && bash Miniconda3-latest-Linux-x86_64.sh -b -p /root/miniconda \
  && rm /tmp/Miniconda3-latest-Linux-x86_64.sh \
  && eval "$(/root/miniconda/bin/conda shell.bash hook)" \
  && conda init \
  && conda install -y numpy tifffile

# Install rawpy with pip into the base environment.
# There's some tomfoolery to start bash and have it pick up the environment.
SHELL ["/root/miniconda/bin/conda", "run", "/bin/bash", "-c"]
RUN pip install rawpy

# Add modified iops to C code.
ADD darktable/src/iop /github/darktable/src/iop

# Add Python wrapper.
COPY py /py
