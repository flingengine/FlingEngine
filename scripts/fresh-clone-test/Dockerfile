# Checks that a fresh clone sets up, builds, and passes its tests on Linux with
# nothing but Init.sh. The container clones the repo you mount at /src (its
# checked-out branch, committed changes only), pulls submodules from GitHub, then
# runs ./Init.sh, a full build, and CTest.
#
# From the repo root:
#   docker build -t fling-fresh-clone scripts/fresh-clone-test
#   docker run --rm -v "$PWD:/src:ro" fling-fresh-clone
#
# On Windows (PowerShell), mount with -v "${PWD}:/src:ro". In Git Bash, prefix
# the run with MSYS_NO_PATHCONV=1 so /src is not rewritten to a Windows path.

FROM ubuntu:24.04

# Build tools plus the same system packages as the Linux CI job
# (.github/workflows/build.yml)
RUN apt-get update -qq \
	&& DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
		ca-certificates git cmake ninja-build build-essential pkg-config \
		xorg-dev libx11-dev libwayland-dev libxkbcommon-dev libglu1-mesa-dev \
		libvulkan-dev \
	&& rm -rf /var/lib/apt/lists/*

COPY entrypoint.sh /usr/local/bin/fling-fresh-clone
# A Windows checkout may have converted the script to CRLF
RUN sed -i 's/\r$//' /usr/local/bin/fling-fresh-clone \
	&& chmod +x /usr/local/bin/fling-fresh-clone

ENTRYPOINT [ "fling-fresh-clone" ]
