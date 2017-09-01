{
  'targets': [
    {
      'target_name': 'appoptics-bindings',
      'include_dirs': [
        "<!(node -e \"require('nan')\")"
      ],
      'sources': [
        'src/bindings.cc'
      ],
      'conditions': [
        ['OS in "linux mac"', {
          'libraries': [
            '-loboe'
          ],
          'ldflags': [
            '-Wl,-rpath-link <!(echo $PWD)/lib',
            '-Wl,-rpath <!(echo $PWD)/lib'
          ]
        }]
      ]
    }
  ]
}
