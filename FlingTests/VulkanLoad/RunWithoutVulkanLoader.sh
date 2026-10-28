#!/usr/bin/env bash
# Run one Sandbox twice with the Vulkan loader not loadable.
# The windowed run must exit 2 and log the missing-runtime line.
# The --null-render run must exit 0. It does not open the loader.
#
# Usage: RunWithoutVulkanLoader.sh <path to Sandbox>

set -euo pipefail

if [[ $# -ne 1 ]]; then
	echo "usage: $0 <Sandbox binary>" >&2
	exit 2
fi

binary=$(realpath "$1")
root=$(cd "$(dirname "$0")/../.." && pwd)

if [[ "$(id -u)" -ne 0 ]]; then
	exec sudo -E bash "$0" "$binary"
fi

exec unshare --mount bash -s -- "$binary" "$root" << 'EOF'
set -euo pipefail
binary=$1
root=$2
cd "$root"

mapfile -t paths < <(ldconfig -p | awk '/libvulkan\.so/{print $NF}' | sort -u)
for path in "${paths[@]}"; do
	if [[ -e "$path" || -L "$path" ]]; then
		mount --bind /dev/null "$path"
	fi
done

set +e
output=$("$binary" --unattended --frames 1 --seed 1 2>&1)
code=$?
set -e
printf '%s\n' "$output"
if [[ "$code" -ne 2 ]]; then
	echo "expected exit code 2 when the Vulkan loader is hidden, got ${code}" >&2
	exit 1
fi
if ! grep -q "Vulkan runtime not found: install a GPU driver with Vulkan support" <<< "$output"; then
	echo "missing Vulkan runtime log line" >&2
	exit 1
fi

"$binary" --null-render --unattended --frames 120 --seed 1
EOF
