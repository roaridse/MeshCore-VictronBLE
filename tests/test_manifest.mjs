import assert from "node:assert/strict";
import { mkdtemp, mkdir, readFile, unlink, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { generatePortalManifest, validateFirmwareVersion, validateTargets } from "../scripts/generate-portal-manifest.mjs";

const configuredTargets = JSON.parse(await readFile(new URL("../portal/targets.json", import.meta.url), "utf8"));
validateTargets(configuredTargets);
assert.deepEqual(
  configuredTargets.packages.map((target) => target.board.id),
  ["rak4631", "heltec-t114", "rak-wismesh-tag"]
);
assert.equal(new Set(configuredTargets.packages.map((target) => target.environment)).size, 3);

const root = await mkdtemp(join(tmpdir(), "meshcore-manifest-"));
const artifacts = join(root, "release-files");
await mkdir(artifacts);
await writeFile(join(artifacts, "Test-Sensor-v1.2.3.uf2"), "uf2 fixture");
await writeFile(join(artifacts, "Test-Sensor-v1.2.3.zip"), "dfu fixture");

const targetsPath = join(root, "targets.json");
await writeFile(targetsPath, JSON.stringify({
  schemaVersion: 1,
  packages: [{
    profile: { id: "test-sensor", name: "Test sensor" },
    board: { id: "test-board", name: "Test board", mcu: "nRF52840", flashMethod: "nrf52-dfu-or-uf2" },
    environment: "test_environment",
    artifactBaseName: "Test-Sensor",
    description: "Manifest generator fixture.",
    features: ["Test feature"],
    validation: "automated",
    artifacts: {
      uf2: "{base}-{version}.uf2",
      dfu: "{base}-{version}.zip"
    }
  }]
}));

const configPath = join(root, "platformio.local.ini");
await writeFile(configPath, `[env:test]\nbuild_flags =\n  -D FIRMWARE_VERSION='"v1.2.3"'\n`);

const manifest = await generatePortalManifest({
  targetsPath,
  artifactsDirectory: artifacts,
  version: "v1.2.3",
  repository: "example/firmware",
  commit: "abc123",
  firmwareConfigPath: configPath,
  generatedAt: "2026-11-21T12:00:00.000Z"
});

assert.equal(manifest.schemaVersion, 1);
assert.equal(manifest.updated, "2026-11-21");
assert.equal(manifest.release.tag, "v1.2.3");
assert.equal(manifest.packages[0].version, "v1.2.3");
assert.equal(manifest.packages[0].assets.uf2.size, 11);
assert.match(manifest.packages[0].assets.uf2.sha256, /^[0-9a-f]{64}$/);
assert.equal(
  manifest.packages[0].assets.dfu.url,
  "https://github.com/example/firmware/releases/download/v1.2.3/Test-Sensor-v1.2.3.zip"
);

await assert.rejects(
  generatePortalManifest({
    targetsPath,
    artifactsDirectory: artifacts,
    version: "not-semver",
    repository: "example/firmware"
  }),
  /SemVer tag/
);
await assert.rejects(validateFirmwareVersion(configPath, "v1.2.4"), /does not match/);

await unlink(join(artifacts, "Test-Sensor-v1.2.3.zip"));
await assert.rejects(generatePortalManifest({
  targetsPath,
  artifactsDirectory: artifacts,
  version: "v1.2.3",
  repository: "example/firmware"
}), /Missing dfu asset/);

console.log("Portal manifest generator tests passed.");
