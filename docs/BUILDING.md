# Building

## GitHub Actions

The workflow in `.github/workflows/build.yml` runs the decoder tests, checks
out the pinned MeshCore revision and builds the RAK4631 firmware. Download the
`RAK4631-Victron-Ruuvi-BLE` artifact from a completed workflow run.

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

The primary artifact is:

```text
.build/MeshCore/.pio/build/RAK_4631_victron_sensor/firmware.zip
```

## GitHub releases and UF2

Every push and pull request builds and verifies `firmware.zip`, `firmware.hex`
and an nRF52840 `firmware.uf2`. A normal build can be downloaded from the
workflow run under **Actions → Build firmware → Artifacts**.

To publish a GitHub Release, first update `FIRMWARE_VERSION` in
`config/platformio.local.ini`, commit and push the change, then create and push
a matching version tag:

```sh
git tag -a v0.2.0 -m "v0.2.0"
git push origin v0.2.0
```

A tag beginning with `v` runs the same tests and build before a separate job is
allowed to create the release. It attaches versioned `.uf2`, `.zip` and `.hex`
files and generates release notes from the commits. Re-running the job replaces
the assets on an existing release.

The UF2 uses the official nRF52840 family ID `0xADA52840` and starts at the
RAK4631 application address `0x26000`. Double-press RESET to enter the RAK4631
UF2 bootloader, then copy the `.uf2` file to the mounted bootloader drive. This
is an application update; it does not install or replace the bootloader.

Run the portable decoder tests separately with:

```sh
./scripts/test.sh
```

The preparation script intentionally refuses to overwrite an existing output
tree. Choose another destination when testing a new clean build:

```sh
./scripts/prepare-meshcore.sh /tmp/meshcore-victron-build
pio run -d /tmp/meshcore-victron-build -e RAK_4631_victron_sensor
```

## Publishing on GitHub

Create an empty GitHub repository, then from this directory run:

```sh
git init -b main
git add .
git commit -m "Initial RAK4631 Victron BLE sensor"
git remote add origin git@github.com:YOUR_USER/MeshCore-VictronBLE.git
git push -u origin main
```

The current managed development environment exposes `.git` as read-only, so
repository initialization must be done outside that environment or after its
special `.git` mount has been removed.
