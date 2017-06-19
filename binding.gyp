{
  "targets": [
    {
      "target_name": "gencore",
      "include_dirs": [ '<!(node -e "require(\'nan\')")' ],
      "sources": [ "src/gencore.cc" ],

      'conditions': [
        ['OS=="linux"', {
          "sources": [ "src/linux.cc" ],
       }],
        ['OS=="mac"', {
          "sources": [ "src/mac.cc" ],
        }
        ]
      ]
    },
    {
      "target_name": "install",
      "type":"none",
      "dependencies" : [ "gencore" ],
      "copies": [
        {
          "destination": "<(module_root_dir)",
          "files": ["<(PRODUCT_DIR)/gencore.node"]
        }]
    },
  ]
}
