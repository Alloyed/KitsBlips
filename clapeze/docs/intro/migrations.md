TODO.

### You don't need a migration to

- Change the display name/description of a parameter, or change its flags.
- Add new parameters (old version of the plugin will ignore unrecognized parameters)
- Change the default
- Increase the range of a parameter.
- Add a new enum value to an existing enum parameter

### You'll need a migration to

- rename a parameter key
- change the curve or range of a NumericParameter (these are 0-1 in data, remember! So you'll need to unmap, then remap)
- decrease the range of a parameter (what should old values do now?)
- reorder an enum parameter
- do harder to quantify custom upgrade logic

### You should not

- reuse a parameter key for a parameter with a completely different type. instead, make a new one.
