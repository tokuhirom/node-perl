test('evaluate', () => {
  const Perl = require('../index').Perl

  expect(Perl).toBeDefined()

  const perl = new Perl()

  expect(perl.evaluate('3')).toBe(3)

  expect(perl.evaluate('3.14')).toBe(3.14)

  expect(perl.evaluate('\'hoge\'x3')).toBe('hogehogehoge')

  expect(perl.evaluate('+[1,2,3]')).toEqual([1, 2, 3])

  expect(perl.evaluate('+{foo => 4, bar => 5}')).toEqual({ foo: 4, bar: 5 })

  expect(perl.evaluate('reverse \'yappo\'')).toBe('oppay')
})
