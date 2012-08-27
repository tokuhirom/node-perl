var test = require('tap').test;
var Perl = require('../index.js').Perl;

test("DynaLoader", function (t) {
    var perl = new Perl();
    t.equivalent(perl.eval("use Socket; 1"), 1, 'DynaLoader');
    t.end();
});
