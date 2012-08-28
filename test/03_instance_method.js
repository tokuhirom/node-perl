var test = require('tap').test;
var Perl = require('../index.js').Perl;

test("bless", function (t) {
    var perl = new Perl();
    var obj = perl.evaluate("package hoge; sub yo { warn q{yo!}; 5963 } sub f2 { warn join(q{...}, @_); }  sub fun2 { shift; return shift()**2 } sub fun3 { (1,2,3,4) } sub fun4 { die } bless [], 'hoge'");
    perl.evaluate("sub p { print @_; \@_ }");
    t.equivalent(obj.yo(), 5963);
    t.equivalent(obj.fun2(3), 9);
    t.equivalent(obj.fun3(), 4);
    t.equivalent(obj.fun3.callList(), [1,2,3,4]);
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
    // t.equivalent(obj.getClassName(), 'hoge');
    console.log(perl);
    t.end();
});
