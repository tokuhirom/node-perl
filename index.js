var P = require('./build/Release/perl.node');
P.Perl.prototype.use = function (name) {
    if (!name.match(/^[A-Za-z0-9_:]+$/)) {
        throw new Exception("This is not a valid class name : " + name);
    }
    return this.evaluate('use ' + name);
};
module.exports.Perl = P.Perl;
