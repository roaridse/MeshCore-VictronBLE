# Building

## GitHub Actions

The workflow in `.github/workflows/build.yml` runs the decoder tests, checks
out the pinned MeshCore revision and builds firmware for all declared boards.
A completed workflow contains `firmware-rak4631`, `firmware-heltec-t114`, and
`firmware-rak-wismesh-tag` artifacts.

Supported portal profiles and boards are declared in `portal/targets.json`.
Tagged releases generate and upload `portal-manifest.json` alongside the UF2,
Nordic DFU ZIP and HEX files. The manifest contains the release URL, size and
SHA-256 digest for every firmware asset and is intended to be consumed by the
MeshCore Sensor Portal.

No keys are required during compilation. All device and channel credentials
are provisioned after flashing, so public CI logs and firmware artifacts do
not contain your secrets.

## Local build

Requirements:

- Git
- Python 3
- PlatformIO 6
- A working ARM GCC runtime

Prepare a fresh upstream tree:

```sh
./scripts/prepare-meshcore.sh
```

Build it:

```sh
./scripts/build.sh
```

Each target produces Nordic DFU ZIP and HEX files under its build directory:

```text
.build/MeshCore/.pio/build/RAK_4631_victron_sensor/firmware.zip
.build/MeshCore/.pio/build/Heltec_T114_victron_sensor/firmware.zip
.build/MeshCore/.pio/build/RAK_WisMesh_Tag_victron_sensor/firmware.zip
```

## GitHub releases and UF2

Every push and pull request builds and verifies `firmware.zip`, `firmware.hex`
and an nRF52840 `firmware.uf2`. A normal build can be downloaded from the
workflow run under **Actions → Build firmware → Artifacts**.

To publish a GitHub Release, first update `FIRMWARE_VERSION` in
`config/platformio.local.ini`, commit and push the change, then create and push
a matching version tag:

```sh
git tag -a v0.5.0 -m "v0.5.0"
git push origin v0.5.0
```

A tag beginning with `v` runs the same tests and build before a separate job is
allowed to create the release. It attaches versioned `.uf2`, `.zip` and `.hex`
files, creates `portal-manifest.json`, and generates release notes from the
commits. The release fails if the tag does not match `FIRMWARE_VERSION` in
`config/platformio.local.ini`. Re-running the job replaces the assets on an
existing release.

Test the manifest generator without building the firmware:

```sh
node tests/test_manifest.mjs
```

If the repository secret `PORTAL_DISPATCH_TOKEN` is configured, a successful
release immediately triggers the Sensor Portal deployment. Without it, the
portal discovers the release during its nightly synchronization. The complete
one-time token setup is documented in the portal repository under
`docs/FIRMWARE_AUTOMATION.md`.

The UF2 files use the official nRF52840 family ID `0xADA52840` and the
SoftDevice 6 application layout used by these three MeshCore board profiles.
Enter the board's UF2 bootloader, then copy only the matching board artifact to
the mounted bootloader drive. This is an application update; it does not
install or replace the bootloader.

Run the portable decoder tests separately with:

```sh
./scripts/test.sh
```

The preparation script intentionally refuses to overwrite an existing output
tree. Choose another destination when testing a new clean build:

```sh
./scripts/prepare-meshcore.sh /tmp/meshcore-victron-build
./scripts/build.sh /tmp/meshcore-victron-build
```

## Publishing on GitHub

Create an empty GitHub repository, then from this directory run:

```sh
git init -b main
git add .
git commit -m "Initial MeshCore BLE sensor firmware"
git remote add origin git@github.com:YOUR_USER/MeshCore-VictronBLE.git
git push -u origin main
```

The current managed development environment exposes `.git` as read-only, so
repository initialization must be done outside that environment or after its
special `.git` mount has been removed.
