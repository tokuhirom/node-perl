{
    'targets': [
        {
            'target_name': 'perl',
            'sources': [
                './src/perl_bindings.cc',
                './src/perlxsi.c'
            ],
            "include_dirs" : [
                "<!(node -e \"require('nan')\")"
            ],
            'libraries': [
                '<!@(perl -MExtUtils::Embed -e ldopts)'
            ],
            'cflags!': [ '-fno-exceptions' ],
            'cflags_cc!': [ '-fno-exceptions' ],
            'conditions': [
                ['OS=="mac"', {
                    'xcode_settings': {
                        'OTHER_LDFLAGS': [
                            '<!@(perl -MExtUtils::Embed -e ldopts)'
                        ],
                        'OTHER_CFLAGS': [
                            '<!@(perl -MExtUtils::Embed -e ccopts)',
                            '<!@(perl utils/libperl.pl)'
                        ]
                    },
                }],
            ],
            'cflags': [
                '<!@(perl -MExtUtils::Embed -e ccopts)',
                '<!@(perl utils/libperl.pl)'
            ]
        },
    ]
}
