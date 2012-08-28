var Perl = require('../index.js').Perl;
var perl = new Perl();
perl.use('LWP::UserAgent');
var ua = perl.getClass('LWP::UserAgent').new();
var res = ua.get('http://mixi.jp/');
console.log(res.as_string());

