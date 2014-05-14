var test = require('tap').test;
var Perl = require('../index.js').Perl;

test("bless", function (t) {
    var perl = new Perl();
    var obj = perl.evaluate("package hoge; sub yo { warn q{yo!}; 5963 } sub f2 { warn join(q{...}, @_); }  sub fun2 { shift; return shift()**2 } sub fun3 { (1,2,3,4) } sub fun4 { die } sub fun5 { shift; my $rh = shift; $rh->{key}; } sub fun6 { shift; shift()->{key1}{key2}{key3}; } bless [], 'hoge'");
    perl.evaluate("sub p { print @_; \@_ }");
    t.equivalent(obj.yo(), 5963);
    t.equivalent(obj.fun2(3), 9);
    t.equivalent(obj.fun3(), 4);
    t.equivalent(obj.fun3.callList(), [1,2,3,4]);
    t.equivalent(obj.fun5({"key" : "value"}), "value");
    t.equivalent(obj.fun6({"key1" : { "key2" : { "key3" : "value"}}}), "value");
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
    console.log(typeof(obj));
    t.equivalent(Perl.blessed(obj), 'hoge');
    console.log(perl);
    t.end();
});

test("bless", function (t) {
    var perl = new Perl();
    var obj = perl.evaluate("bless [4,6,4,9], 'hoge'");
    perl.evaluate("use Scalar::Util qw/blessed/; sub p { blessed(shift) }");
    perl.evaluate("sub nop { shift }");
    t.equivalent(perl.call('p', obj), 'hoge');
    t.equivalent(perl.call('nop', perl.getClass("YO")), 'YO');
    console.log(perl);
    t.end();
});

test("blessed", function (t) {
    var perl = new Perl();
    var obj = perl.evaluate("bless [4,6,4,9], 'hoge'");
    t.equivalent(Perl.blessed(obj), 'hoge');
    t.equivalent(Perl.blessed(perl.getClass("YO")), undefined);
    console.log(perl);
    t.end();
});
