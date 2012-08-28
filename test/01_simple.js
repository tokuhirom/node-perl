var test = require('tap').test;
var Perl = require('../index.js').Perl;

test("", function (t) {
    var perl = new Perl();
    t.ok(perl);
    t.equivalent(perl.evaluate("3"), 3);
    t.equivalent(perl.evaluate("3.14"), 3.14);
    t.equivalent(perl.evaluate("'hoge'x3"), 'hogehogehoge');
    t.equivalent(perl.evaluate("+[1,2,3]"), [1,2,3]);
    t.equivalent(perl.evaluate("+{foo => 4, bar => 5}"), {foo:4, bar:5});
    t.equivalent(perl.evaluate("reverse 'yappo'"), 'oppay');
    t.end();
});

test("eval", function (t) {
    t.plan(2);
    var perl = new Perl();
    var obj = perl.evaluate("package hoge; sub yo { warn q{yo!}; 5963 } sub f2 { warn join(q{...}, @_); }  sub fun2 { shift; return shift()**2 } sub fun3 { (1,2,3,4) } sub fun4 { die } bless [], 'hoge'");
    try {
        obj.fun4();
    } catch (e) {
        t.ok(e.match(/Died/), 'died');
    }
    try {
        obj.fun4.callList();
    } catch (e) {
        t.ok(e.match(/Died/), 'died');
    }
    t.end();
});

test("func", function (t) {
    var perl = new Perl();
    perl.evaluate("sub foo { 4**2 }");
    perl.evaluate("sub bar { return (5,9,6,3) }");
    t.equivalent(perl.call('foo'), 16);
    t.equivalent(perl.call('bar'), 3);
    t.equivalent(perl.callList('bar'), [5,9,6,3]);
    t.end();
});

test("class", function (t) {
    var perl = new Perl();
    perl.evaluate("package Foo; sub foo { 4**2 }");
    perl.evaluate("package Foo; sub bar { $_[0]x3 }");
    perl.evaluate("package Foo; sub baz { $_[1]*2 }");
    t.equivalent(perl.getClass('Foo').call('foo'), 16);
    t.equivalent(perl.getClass('Foo').call('bar'), "FooFooFoo");
    t.equivalent(perl.getClass('Foo').call('baz', 5), 10);
    t.equivalent(perl.getClass('Foo').getClassName(), 'Foo');
    t.end();
});

test("gc", function (t) {
    var gc;
    try {
        gc = require("gc");
    } catch(e) { }
    if (gc) {
        var perl = new Perl();
        perl.evaluate("sub foo { 4**2 }");
        perl.evaluate("sub bar { return (5,9,6,3) }");
        t.equivalent(perl.call('foo'), 16);
        t.equivalent(perl.call('bar'), 3);
        t.equivalent(perl.callList('bar'), [5,9,6,3]);
        gc();
    }
    t.end();
});

