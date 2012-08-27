var test = require('tap').test;
var Perl = require('../index.js').Perl;

test("", function (t) {
    var perl = new Perl();
    t.ok(perl);
    t.equivalent(perl.eval("3"), 3);
    t.equivalent(perl.eval("3.14"), 3.14);
    t.equivalent(perl.eval("'hoge'x3"), 'hogehogehoge');
    t.equivalent(perl.eval("+[1,2,3]"), [1,2,3]);
    t.equivalent(perl.eval("+{foo => 4, bar => 5}"), {foo:4, bar:5});
    t.end();
});
