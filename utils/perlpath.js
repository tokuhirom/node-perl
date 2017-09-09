const path = require('path');
const os = require('os');
const fs = require('fs');

if(os.platform() !== 'win32') {
    process.stderr.write('only supports win32\n');
    process.exit(1);
}

const PATH = [
    ...(
        typeof process.env.PERL_HOME === 'string' 
        ? [ process.env.PERL_HOME ] 
        : []
    ),
    ...(
        typeof process.env.PATH === 'string' 
        ? process.env.PATH.split(';') 
        : []
    ),
].reduce((paths, cpath) => paths.concat([
    cpath,
    path.resolve(cpath, './lib/CORE'),
    path.resolve(cpath, '../lib/CORE'),
]), []);

function hasPerlSource(dir) {
    try {
        return fs.readdirSync(dir).includes('perl.h');
    } catch(err) {
        // skip over non-existent directories or ones we dont have access to
    }
    return false;
}

const PERL_PATH = PATH.find((cpath) => hasPerlSource(cpath));

if(!PERL_PATH) {
    process.stderr.write('perl source was not found on path.\n');
    process.exit(1);
}

fs.appendFileSync(path.resolve(__dirname, '../src/config.h'), `#define LIBPERL_DIR "${PERL_PATH.split('\\').join('\\\\')}\\\\"\n`);

process.stdout.write(`${PERL_PATH}\n`);

