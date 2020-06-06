{
  "variables": {
    "v8_enable_pointer_compression": "false",
    "v8_enable_31bit_smis_on_64bit_arch": "false"
  },
  "targets": [
    {
      "target_name": "perl",
      "defines": [],
      "sources": [
        "./src/perl_bindings.cc",
        "./src/perlxsi.c"
      ],
      "include_dirs": [
        "<!(node -e \"require('nan')\")",
        "./extern/"
      ],
      "libraries": [
        "<!@(node utils/ldopts.js)"
      ],
      "cflags!": [
        "-fno-exceptions",
        "-Wno-literal-suffix"
      ],
      "cflags_cc!": [
        "-fno-exceptions",
        "-Wno-literal-suffix"
      ],
      "cflags": [
        "<!@(perl -MExtUtils::Embed -e ccopts)",
        "<!@(perl utils/libperl.pl)"
      ],
      "ccflags": [
        "-fno-exceptions",
        "-Wno-literal-suffix"
      ],
      "conditions": [
        [
          "OS=='win'",
          {
            "include_dirs": [
              "<!@(perl utils/libperl.pl)",
              "<!(node utils/perlpath.js)"
            ],
            "defines": [
              "__inline__=__inline"
            ],
            "libraries": [
              "-lpsapi.lib"
            ]
          }
        ],
        [
          "OS=='mac'",
          {
            "xcode_settings": {
              "OTHER_LDFLAGS": [
                "<!@(perl -MExtUtils::Embed -e ldopts)"
              ],
              "OTHER_CFLAGS": [
                "<!@(perl -MExtUtils::Embed -e ccopts)",
                "<!@(perl utils/libperl.pl)"
              ]
            }
          }
        ]
      ]
    }
  ]
}