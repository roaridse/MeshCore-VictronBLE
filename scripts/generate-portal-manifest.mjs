#!/usr/bin/env node

import { createHash } from "node:crypto";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const SEMVER_TAG = /^v\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$/;
const ASSET_TYPES = new Set(["uf2", "dfu", "hex", "bin"]);

export async function generatePortalManifest({
  targetsPath,
  artifactsDirectory,
  version,
  repository,
  commit = "",
  firmwareConfigPath = null,
  generatedAt = new Date().toISOString()
}) {
  validateVersion(version);
  validateRepository(repository);

  const targets = JSON.parse(await readFile(targetsPath, "utf8"));
  validateTargets(targets);
  if (firmwareConfigPath) await validateFirmwareVersion(firmwareConfigPath, version);

  const seen = new Set();
  const packages = [];
  for (const target of targets.packages) {
    const identity = `${target.profile.id}/${target.board.id}/${target.environment}/${version}`;
    if (seen.has(identity)) throw new Error(`Duplicate portal package: ${identity}`);
    seen.add(identity);

    const assets = {};
    for (const [type, template] of Object.entries(target.artifacts)) {
      if (!ASSET_TYPES.has(type)) throw new Error(`Unsupported asset type '${type}' in ${identity}`);
      const filename = renderArtifactName(template, target.artifactBaseName, version);
      const filePath = join(artifactsDirectory, filename);
      const contents = await readFile(filePath).catch((error) => {
        throw new Error(`Missing ${type} asset for ${identity}: ${filePath} (${error.code || error.message})`);
      });
      assets[type] = {
        url: releaseAssetUrl(repository, version, filename),
        sha256: createHash("sha256").update(contents).digest("hex"),
        size: contents.byteLength
      };
    }

    packages.push({
      profile: target.profile,
      board: target.board,
      version,
      environment: target.environment,
      description: target.description,
      features: target.features,
      validation: target.validation,
      source: `https://github.com/${repository}`,
      assets
    });
  }

  return {
    schemaVersion: 1,
    updated: generatedAt.slice(0, 10),
    release: {
      tag: version,
      repository,
      commit,
      generatedAt
    },
    packages
  };
}

export function validateTargets(targets) {
  if (targets?.schemaVersion !== 1 || !Array.isArray(targets.packages) || targets.packages.length === 0) {
    throw new Error("targets.json must use schemaVersion 1 and contain at least one package");
  }
  for (const [index, target] of targets.packages.entries()) {
    const prefix = `packages[${index}]`;
    requireString(target?.profile?.id, `${prefix}.profile.id`);
    requireString(target?.profile?.name, `${prefix}.profile.name`);
    requireString(target?.board?.id, `${prefix}.board.id`);
    requireString(target?.board?.name, `${prefix}.board.name`);
    requireString(target?.board?.mcu, `${prefix}.board.mcu`);
    requireString(target?.board?.flashMethod, `${prefix}.board.flashMethod`);
    requireString(target?.environment, `${prefix}.environment`);
    requireString(target?.artifactBaseName, `${prefix}.artifactBaseName`);
    requireString(target?.description, `${prefix}.description`);
    requireString(target?.validation, `${prefix}.validation`);
    if (!Array.isArray(target.features) || target.features.some((feature) => typeof feature !== "string" || !feature.trim())) {
      throw new Error(`${prefix}.features must be an array of non-empty strings`);
    }
    if (!target.artifacts || typeof target.artifacts !== "object" || Array.isArray(target.artifacts) || Object.keys(target.artifacts).length === 0) {
      throw new Error(`${prefix}.artifacts must contain at least one asset template`);
    }
    for (const [type, template] of Object.entries(target.artifacts)) {
      requireString(template, `${prefix}.artifacts.${type}`);
      if (!template.includes("{version}")) throw new Error(`${prefix}.artifacts.${type} must contain {version}`);
    }
  }
}

export async function validateFirmwareVersion(configPath, version) {
  const config = await readFile(configPath, "utf8");
  const matches = [...config.matchAll(/-D\s+FIRMWARE_VERSION\s*=\s*['"]?\\?['"](v[^'"\\\s]+)\\?['"]?/g)];
  const configuredVersions = [...new Set(matches.map((match) => match[1]))];
  if (configuredVersions.length !== 1) {
    throw new Error(`Expected exactly one FIRMWARE_VERSION in ${configPath}; found ${configuredVersions.length}`);
  }
  if (configuredVersions[0] !== version) {
    throw new Error(`Release tag ${version} does not match firmware FIRMWARE_VERSION ${configuredVersions[0]}`);
  }
}

function validateVersion(version) {
  if (!SEMVER_TAG.test(version || "")) throw new Error(`Version must be a SemVer tag such as v0.4.0; received '${version || ""}'`);
}

function validateRepository(repository) {
  if (!/^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$/.test(repository || "")) {
    throw new Error(`Repository must use owner/name format; received '${repository || ""}'`);
  }
}

function requireString(value, field) {
  if (typeof value !== "string" || !value.trim()) throw new Error(`${field} must be a non-empty string`);
}

function renderArtifactName(template, base, version) {
  const filename = template.replaceAll("{base}", base).replaceAll("{version}", version);
  if (basename(filename) !== filename || filename.includes("..")) throw new Error(`Unsafe artifact filename: ${filename}`);
  return filename;
}

function releaseAssetUrl(repository, version, filename) {
  return `https://github.com/${repository}/releases/download/${encodeURIComponent(version)}/${encodeURIComponent(filename)}`;
}

function parseArguments(argv) {
  const values = {};
  for (let index = 0; index < argv.length; index += 2) {
    const key = argv[index];
    const value = argv[index + 1];
    if (!key?.startsWith("--") || value == null) throw new Error(`Expected --name value, received '${key || ""}'`);
    values[key.slice(2)] = value;
  }
  return values;
}

async function main() {
  const args = parseArguments(process.argv.slice(2));
  const required = ["targets", "artifacts", "output", "version", "repository"];
  for (const name of required) {
    if (!args[name]) throw new Error(`Missing required argument --${name}`);
  }
  const manifest = await generatePortalManifest({
    targetsPath: args.targets,
    artifactsDirectory: args.artifacts,
    version: args.version,
    repository: args.repository,
    commit: args.commit || process.env.GITHUB_SHA || "",
    firmwareConfigPath: args["firmware-config"] || null,
    generatedAt: args["generated-at"] || new Date().toISOString()
  });
  await mkdir(dirname(args.output), { recursive: true });
  await writeFile(args.output, `${JSON.stringify(manifest, null, 2)}\n`);
  console.log(`Generated ${args.output}: ${manifest.packages.length} package(s)`);
}

const invokedPath = process.argv[1] ? resolve(process.argv[1]) : "";
if (invokedPath && fileURLToPath(import.meta.url) === invokedPath) {
  main().catch((error) => {
    console.error(error.message);
    process.exitCode = 1;
  });
}
