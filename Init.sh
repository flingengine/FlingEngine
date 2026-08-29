#!/bin/sh
echo   =============================================================
echo ================= Fling Engine initalize script =================
echo   =============================================================

# Make sure that we have all external libraries that we need
echo Initalizing submodules...
git submodule update --init --recursive

# Install Catch2 so that CMake can find it (installed locally so no sudo is needed)
echo Installing Catch2 testing library...
cmake ./external/Catch2 -B ./external/Catch2/build -DCMAKE_INSTALL_PREFIX=./external/Catch2/install
make install --directory=./external/Catch2/build -j$(nproc)

# Run cmake!
echo Running CMake
cmake . -B build

# Offer to set up the Fling Engine Claude Code skills marketplace, if Claude Code is
# installed and the marketplace isn't already registered
if command -v claude >/dev/null 2>&1; then
	if claude plugin marketplace list --json 2>/dev/null | grep -q '"name": "fling-engine-skills"'; then
		echo Fling Engine skills marketplace already registered, skipping.
	else
		printf "Claude Code detected. Add the Fling Engine skills marketplace (https://github.com/flingengine/skills-marketplace)? [y/N] "
		read -r add_marketplace
		case "$add_marketplace" in
			[yY]|[yY][eE][sS])
				claude plugin marketplace add flingengine/skills-marketplace
				;;
			*)
				echo Skipping Claude Code skills marketplace setup.
				;;
		esac
	fi
fi
