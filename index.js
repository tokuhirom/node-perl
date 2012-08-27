var P = require('./build/Release/perl-simple.node');
P.Perl.prototype.eval = function () {
    var ret = this._eval.apply(this, Array.prototype.slice.apply(arguments));
    if (typeof ret === 'Object') {
        if (ret['HOGEHOGE']) {
            console.log("AO");
        } else {
            return ret;
        }
    } else {
        return ret;
    }
};
module.exports.Perl = P.Perl;
