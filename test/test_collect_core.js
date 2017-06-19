const gencore = require('../');
const fs = require('fs');
const tap = require('tap');
const zlib = require('zlib');
const tar = require('tar');

var core_count = 0;
let callback_count = 0;

tap.comment('Creating core and collecting libraries.');
gencore.collectCore(checkTarGz);

function checkTarGz(error, filename) {
  callback_count++;
  tap.ok(error === null, 'Error argument should be null');
  tap.ok(callback_count == 1, 'Callback should only be called once on success.');
  tap.ok(fs.existsSync(filename), `Collected core and libraries file ${filename} exists.`);

  tap.match(filename, /core_[0-9]{8}.[0-9]{6}\.[0-9]+\.[0-9]{3}\.tar\.gz/, 'Filename pattern should be core_YYYYMMDD.HHMMSS.<pid>.<seq>.tar.gz');

  //Get the list of files in the tar.gz so we can check them in subsequent tests.
  const read = fs.createReadStream(filename);
  const gunzip = zlib.createGunzip();
  const parse = tar.Parse();

  // Setup the plumbing for checking the results.
  parse.on('entry', checkEntry);
  parse.on('end', checkCoreExists);

  // Read the tar.gz file and the callbacks above should do the checks.
  read.pipe(gunzip).pipe(parse);
}

function checkEntry(entry) {
  var name = entry.path;
  var size = entry.size;
  var type = entry.type;

  // Check there's a file in the root that has a core-ish name.
  // TODO - How do I know how many files there are and when I'm done?
  // TODO - This will break on Mac with /cores but that's ok I want to check it works.
  if( name.startsWith('core')) {
    core_count++;
  }

  // Check no file in the core starts with / so we can't overwrite system files on
  // extraction. (Unless root extracts in /!)
  tap.notOk( name.startsWith('/'), 'Check for relative paths in tar: ' + name);

  // Check any file in the tar has a non-zero size.
  if( type == 'File') {
    tap.notOk(size == 0, 'File size > 0 bytes: ' + name);
  } else if (type == 'Directory') {
    tap.notOk( size != 0, 'Directory size == 0 bytes: ' + name);
  } else {
    tap.fail('Only files and directories in the tar file: ' + name);
  }
}

function checkCoreExists() {
  tap.ok(core_count === 1, 'Check we have only one core file, found ' + core_count);
}
