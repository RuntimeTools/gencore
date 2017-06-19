const gencore = require('../');
const fs = require('fs');
const tap = require('tap');

tap.comment('Creating core and collecting libraries.');
gencore.createCore(checkCore);

function checkCore(error, filename) {
  tap.ok(error === null, 'Error object should be null');
  tap.ok(fs.existsSync(filename), `Found core file ${filename}.`);
}

// TODO - Test with ulimit of 0?
// Need to set a hard ulimit, might need a wrapper child process so we don't break this shell.
