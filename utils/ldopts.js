const exec = require('child_process').exec
const os = require('os')

exec('perl -MExtUtils::Embed -e ldopts', (error, stdout) => {
  if (error) {
    console.error(error.message)
    process.exit(1)
  }

  if (os.platform() === 'win32') {
    stdout = stdout.split(' ')
      .filter(elem => elem.indexOf('.a') !== -1)
      .join('')
  }

  console.log(stdout)
})
