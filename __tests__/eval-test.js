const assert = require('assert')

test('eval', () => {
  const Perl = require('../index').Perl

  expect(Perl).toBeDefined()

  const perl = new Perl()

  const obj = perl.evaluate(
      `package hoge;

        sub yo {
            warn q{yo!};
            5963
        }

        sub f2 {
            warn join(q{...}, @_);
        }

        sub fun2 {
            shift;
            return shift()**2
        }

        sub fun3 {
            (1,2,3,4)
        }

        sub fun4 {
            die
        }

        bless [], 'hoge'`
  )

  try {
    obj.fun4()
  } catch (e) {
    assert(e.match(/Died/)[0] === 'Died')
  }

  try {
    obj.fun4.callList()
  } catch (e) {
    assert(e.match(/Died/)[0] === 'Died')
  }
})
