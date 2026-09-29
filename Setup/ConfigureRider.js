// Copyright (c) 2026 Nelaric
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { execFileSync } = require("node:child_process");

const project = path.resolve(__dirname, "../NelaricGameplay");
const editorPackage = JSON.parse(fs.readFileSync(path.join(project,
  "Content/JavaScript/PuertsEditor/package.json"), "utf8"));
const typescriptVersion = editorPackage.dependencies.typescript;

function readJson(file) {
  // TypeScript configuration permits comments and trailing commas.
  const ts = require(path.join(project, "Content/JavaScript/PuertsEditor/node_modules/typescript"));
  const result = ts.readConfigFile(file, (name) => fs.readFileSync(name, "utf8"));
  if (result.error) {
    throw new Error(ts.flattenDiagnosticMessageText(result.error.messageText, "\n"));
  }
  return result.config;
}

function writeJson(file, value) {
  fs.writeFileSync(file, JSON.stringify(value, null, 2) + "\n", "utf8");
}

function createMissing(relative, content) {
  const file = path.join(project, relative);
  fs.mkdirSync(path.dirname(file), { recursive: true });
  if (!fs.existsSync(file)) {
    fs.writeFileSync(file, content, { encoding: "utf8", flag: "wx" });
  }
}

const packageFile = path.join(project, "package.json");
const manifest = fs.existsSync(packageFile) ? JSON.parse(fs.readFileSync(packageFile, "utf8")) : {
  name: "nelaric-gameplay-scripts", private: true,
};
manifest.scripts ??= {};
for (const [name, command] of Object.entries({
  typecheck: "tsc --noEmit", build: "tsc", watch: "tsc --watch",
})) {
  manifest.scripts[name] ??= command;
}
if (!manifest.dependencies?.typescript && !manifest.devDependencies?.typescript) {
  manifest.devDependencies ??= {};
  manifest.devDependencies.typescript = typescriptVersion;
}
writeJson(packageFile, manifest);

const configFile = path.join(project, "tsconfig.json");
if (!fs.existsSync(configFile)) {
  writeJson(configFile, {
    compilerOptions: {
      target: "ES2020", module: "commonjs", moduleResolution: "node", strict: true,
      experimentalDecorators: true, useDefineForClassFields: false,
      sourceMap: true, inlineSources: true, rootDir: "TypeScript", outDir: "Content/JavaScript",
      typeRoots: ["Typing", "node_modules/@types"],
    },
    include: ["TypeScript/**/*.ts"], exclude: ["node_modules", "Content/JavaScript"],
  });
} else {
  const config = readJson(configFile);
  const missing = Object.fromEntries(Object.entries({
    strict: true, moduleResolution: "node", inlineSources: true,
  }).filter(([name]) => config.compilerOptions?.[name] === undefined && !config.extends));
  if (Object.keys(missing).length) {
    // Insert defaults through TypeScript's JSONC syntax tree, retaining comments.
    const ts = require(path.join(project, "Content/JavaScript/PuertsEditor/node_modules/typescript"));
    const text = fs.readFileSync(configFile, "utf8");
    const tree = ts.parseJsonText(configFile, text);
    const root = tree.statements[0].expression;
    const options = root.properties.find((property) => property.name?.text === "compilerOptions")?.initializer;
    const object = options ?? root;
    const position = object.end - 1;
    const last = object.properties[object.properties.length - 1];
    const comma = last && !object.properties.hasTrailingComma ? "," : "";
    const first = object.properties[0];
    const propertyLine = first ? text.slice(0, first.getStart(tree)).split(/\r?\n/).pop() : "";
    const indent = /^\s*$/.test(propertyLine) && propertyLine.length ? propertyLine : "  ";
    const closingIndent = indent.slice(0, Math.max(0, indent.length - 2));
    const additions = options ? Object.entries(missing).map(([key, value]) =>
      indent + JSON.stringify(key) + ": " + JSON.stringify(value)).join(",\n") :
      indent + '"compilerOptions": ' + JSON.stringify(missing, null, 2);
    const withComma = last ? text.slice(0, last.end) + comma + text.slice(last.end, position) : text.slice(0, position);
    fs.writeFileSync(configFile, withComma.trimEnd() + "\n" + additions + "\n" + closingIndent + text.slice(position), "utf8");
  }
  if (config.compilerOptions?.sourceMap !== true && !config.extends) {
    console.warn("Enable sourceMap in tsconfig.json to debug TypeScript source files.");
  }
}

createMissing("TypeScript/Entry.ts",
  '// Copyright (c) 2026 Nelaric\nconsole.log("Nelaric TypeScript entry started.");\nexport {};\n');

const iniFile = path.join(project, "Config/DefaultPuerts.ini");
let ini = fs.readFileSync(iniFile, "utf8");
const section = /^\[\/Script\/Puerts\.PuertsSetting\][^\r\n]*\r?\n([\s\S]*?)(?=^\[|(?![\s\S]))/m;
const match = ini.match(section);
if (!match) throw new Error("DefaultPuerts.ini has no Puerts settings section.");
let settings = match[1];
for (const [name, value] of Object.entries({ DebugEnable: "True", DebugPort: "8080", WaitDebugger: "False" })) {
  if (!new RegExp(`^${name}\\s*=`, "mi").test(settings)) settings += `${name}=${value}\n`;
}
ini = ini.replace(section, (whole) => whole.slice(0, whole.length - match[1].length) + settings);
fs.writeFileSync(iniFile, ini, "utf8");
const portMatch = settings.match(/^DebugPort\s*=\s*(\d+)\s*$/mi);
if (!portMatch || Number(portMatch[1]) < 1 || Number(portMatch[1]) > 65535) {
  throw new Error("Puerts DebugPort must be between 1 and 65535.");
}
const port = portMatch[1];
const gameIniFile = path.join(project, "Config/DefaultGame.ini");
let gameIni = fs.existsSync(gameIniFile) ? fs.readFileSync(gameIniFile, "utf8") :
  "; Copyright (c) 2026 Nelaric\n";
if (!/^\+?DirectoriesToAlwaysStageAsUFS\s*=\s*\(Path="JavaScript"\)/mi.test(gameIni)) {
  const packaging = /^\[\/Script\/UnrealEd\.ProjectPackagingSettings\][^\r\n]*\r?\n/m;
  const entry = '+DirectoriesToAlwaysStageAsUFS=(Path="JavaScript")\n';
  gameIni = packaging.test(gameIni) ? gameIni.replace(packaging, (header) => header + entry) :
    gameIni.trimEnd() + "\n\n[/Script/UnrealEd.ProjectPackagingSettings]\n" + entry;
  fs.writeFileSync(gameIniFile, gameIni, "utf8");
}
createMissing(".run/Puerts Attach.run.xml", `<!-- Copyright (c) 2026 Nelaric -->
<component name="ProjectRunConfigurationManager">
  <configuration default="false" name="Puerts Attach" type="ChromiumRemoteDebugType" factoryName="Chromium Remote" host="localhost" port="${port}">
    <method v="2" />
  </configuration>
</component>
`);

// Resolve npm-cli through Node on Windows, avoiding cmd.exe path interpolation.
if (process.platform === "win32") {
  const npmPath = execFileSync("where.exe", ["npm.cmd"], { encoding: "utf8" }).trim().split(/\r?\n/)[0];
  execFileSync(process.execPath, [path.join(path.dirname(npmPath), "node_modules/npm/bin/npm-cli.js"),
    "install", "--include=dev"], { cwd: project, stdio: "inherit" });
} else {
  execFileSync("npm", ["install", "--include=dev"], { cwd: project, stdio: "inherit" });
}
console.log("Rider tooling prepared. Enable JavaScript and TypeScript, JavaScript Debugger, and Node.js in Rider.");
console.log("Generate UE declarations in the editor, then run npm run typecheck and npm run build in NelaricGameplay.");
console.log(`Attach to the running Puerts environment at localhost:${port}. Existing settings are preserved.`);
console.log("Entry.ts must be started by a game-owned FJsEnv; see Setup/README.md for runtime and packaging steps.");
