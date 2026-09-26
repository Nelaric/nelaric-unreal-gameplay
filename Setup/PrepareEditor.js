// Copyright (c) 2026 Nelaric
"use strict";

const fs = require("node:fs");
const path = require("node:path");

const project = path.resolve(__dirname, "../NelaricGameplay");
const source = path.join(project, "Plugins/Puerts/Content/JavaScript");
const destination = path.join(project, "Content/JavaScript");

function copyMissing(from, to) {
  fs.mkdirSync(to, { recursive: true });
  for (const entry of fs.readdirSync(from, { withFileTypes: true })) {
    const sourcePath = path.join(from, entry.name);
    const targetPath = path.join(to, entry.name);
    if (entry.isDirectory()) {
      copyMissing(sourcePath, targetPath);
    } else if (entry.isFile() && !fs.existsSync(targetPath)) {
      fs.copyFileSync(sourcePath, targetPath, fs.constants.COPYFILE_EXCL);
    }
  }
}

copyMissing(source, destination);
