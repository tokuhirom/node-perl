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
    t.equivalent(perl.eval("reverse 'yappo'"), 'oppay');
    t.end();
});

test("bless", function (t) {
    var perl = new Perl();
    var obj = perl.eval("package hoge; sub yo { warn q{yo!}; 5963 } sub f2 { warn join(q{...}, @_); }  sub fun2 { shift; return shift()**2 } sub fun3 { (1,2,3,4) } sub fun4 { die } bless [], 'hoge'");
    t.equivalent(obj.call('yo'), 5963);
    t.equivalent(obj.call('fun2', 3), 9);
    t.equivalent(obj.call('fun2', 3), 9);
    t.equivalent(obj.call('fun3'), 4);
    t.equivalent(obj.callList('fun3'), [1,2,3,4]);
    try {
        obj.call('fun4');
    } catch (e) {
        t.ok(e.match(/Died/), 'died');
    }
    try {
        obj.callList('fun4');
    } catch (e) {
        t.ok(e.match(/Died/), 'died');
    }
    t.equivalent(obj.getClassName(), 'hoge');
    console.log(perl);
    t.end();
});

test("eval", function (t) {
    t.plan(2);
    var perl = new Perl();
    var obj = perl.eval("package hoge; sub yo { warn q{yo!}; 5963 } sub f2 { warn join(q{...}, @_); }  sub fun2 { shift; return shift()**2 } sub fun3 { (1,2,3,4) } sub fun4 { die } bless [], 'hoge'");
    try {
        obj.call('fun4');
    } catch (e) {
        t.ok(e.match(/Died/), 'died');
    }
    try {
        obj.callList('fun4');
    } catch (e) {
        t.ok(e.match(/Died/), 'died');
    }
    t.end();
});

