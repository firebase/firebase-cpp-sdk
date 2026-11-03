#!/usr/bin/env node
// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

const {spawn, spawnSync} = require('child_process');
const http = require('http');
const os = require('os');
const path = require('path');

const functionsDir = path.resolve(__dirname, 'functions');
const npmBin = os.platform() === 'win32' ? 'npm.cmd' : 'npm';
const npmInstall = spawnSync(npmBin, ['install'], {
  cwd: functionsDir,
  stdio: 'inherit',
});
if (npmInstall.error || npmInstall.status !== 0) {
  console.error('Failed to install functions npm dependencies.');
  process.exit(npmInstall.status || 1);
}

const firebaseBin = path.resolve(
    functionsDir,
    'node_modules/.bin',
    os.platform() === 'win32' ? 'firebase.cmd' : 'firebase'
);

const emulator = spawn(
    firebaseBin,
    ['emulators:start', '--only', 'functions', '--project', 'functions-integration-test'],
    {
      cwd: __dirname,
      stdio: 'inherit',
    }
);

function shutdown(signal) {
  if (!emulator.killed) {
    emulator.kill(signal || 'SIGTERM');
  }
}

process.on('SIGINT', () => shutdown('SIGINT'));
process.on('SIGTERM', () => shutdown('SIGTERM'));
process.on('exit', () => shutdown('SIGTERM'));

emulator.on('exit', (code) => {
  process.exit(code || 0);
});
