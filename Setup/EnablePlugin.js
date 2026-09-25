// Copyright (c) 2026 Nelaric
"use strict";

const fs = require("node:fs");
const path = require("node:path");

const project = path.resolve(__dirname, "../NelaricGameplay");
const descriptor = path.join(project, "NelaricGameplay.uproject");
const backend = path.join(project, "Plugins/Puerts/ThirdParty/v8_9.4.146.24");
const required = [
  path.join(backend, "Inc/v8.h"),
  path.join(backend, "LICENSE"),
  path.join(project, "tsconfig.json"),
  path.join(project, "Config/DefaultPuerts.ini"),
  path.join(project, "Content/JavaScript/PuertsEditor/node_modules/typescript/package.json"),
];

for (const file of required) {
  if (!fs.existsSync(file)) {
    throw new Error(`PuerTS setup is incomplete: ${file}`);
  }
}

const source = fs.readFileSync(descriptor, "utf8");
const parsed = JSON.parse(source);
const entries = parsed.Plugins.filter((plugin) => plugin.Name === "Puerts");
if (entries.length !== 1) {
  throw new Error("Expected exactly one Puerts entry in NelaricGameplay.uproject");
}
if (entries[0].Enabled === true) {
  console.log("PuerTS is already enabled.");
  process.exit(0);
}
if (entries[0].Enabled !== false) {
  throw new Error("Expected Puerts to be explicitly disabled before setup");
}

const pattern = /("Name"\s*:\s*"Puerts"\s*,\s*"Enabled"\s*:\s*)false/;
if (!pattern.test(source)) {
  throw new Error("Could not locate the Puerts enabled setting");
}
fs.writeFileSync(descriptor, source.replace(pattern, "$1true"), "utf8");
console.log("Enabled PuerTS in NelaricGameplay.uproject.");
