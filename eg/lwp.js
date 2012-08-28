var Perl = require('../index.js').Perl;
var perl = new Perl();
perl.use('LWP::UserAgent');
var ua = perl.getClass('LWP::UserAgent').call('new');
var res = ua.call('get', 'http://mixi.jp/');
console.log(res.call('as_string'));

