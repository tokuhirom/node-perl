const P = require('bindings')('perl')
P.InitPerl()

P.Perl.prototype.use = function (name) {
  if (!name.match(/^[A-Za-z0-9_:]+$/)) {
    throw new Error('This is not a valid class name : ' + name)
  }
  return this.evaluate('use ' + name)
}

module.exports.Perl = P.Perl
